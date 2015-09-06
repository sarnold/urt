/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notices are 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */

/* 
 * map_scan.c - Put RLE images on X display.
 * 
 * Author:	Spencer W. Thomas  (x10)
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Feb 20 1986
 * Copyright (c) 1986, University of Utah
 * 
 * Modified:	Andrew F. Vesper (x 11)
 *		High Performance Workstations
 *		Digital Equipment Corp
 * Date:	Fri, Aug 7, 1987
 *		Thu, Jan 14, 1988
 * Copyright (c) 1987,1988, Digital Equipment Corporation
 * 
 * Modified:	Martin R. Friedmann (better X11, flipbook, MAG, info)
 * 		Dept of Electrical Engineering and Computer Science
 *		University of Michigan
 * Date:	Tue, Nov 14, 1989
 * Copyright (c) 1989, University of Michigan
 */

#include "getx11.h"

static void map_scanline_generic();
static void map_1_dither_notable_1();
static void map_1_dither_table_8();
static void map_1_nodither_table_8();
static void map_2or3_dither_table_8();
static void map_1_nodither_notable_32();
static void map_2or3_nodither_table_8();
static void map_2or3_dither_notable_32();
static void map_2or3_nodither_notable_32();
static void map_1_mono_color_8();
static void map_1_mono_color_32();
static void MAG_scanline_generic();
static void MAG_1_dither_notable_1();
static void MAG_1_dither_table_8();
static void MAG_1_nodither_table_8();
static void MAG_1_nodither_notable_32();
static void MAG_2or3_dither_table_8();
static void MAG_2or3_nodither_table_8();
static void MAG_2or3_dither_notable_32();
static void MAG_2or3_nodither_notable_32();
static void MAG_1_mono_color_8();
static void MAG_1_mono_color_32();

static unsigned char LSBMask[9] = { 0x0, 0x1, 0x3, 0x7,
				    0xf, 0x1f, 0x3f, 0x7f, 0xff };
#if 0				/* Not actually used. */
static unsigned char MSBMask[9] = { 0x00, 0x80, 0xc0, 0xe0,
				    0xf0, 0xf8, 0xfc, 0xfe, 0xff };
#endif

/*
 * Figure out which scan line routine to use.  
 */
void choose_scanline_converter( img )
image_information *img;
{
    register int i;
    register Boolean table_present;
    static struct {
	Boolean	one_color;
	Boolean	dither;
	Boolean	table_present;
	int	bpp;
	void	(*routine)();
	void	(*mag_routine)();
    } map_scanline_table[] =
    {
	
	/* 1 color, dithr, Table, b/p, routine ptr */
	
    {  True, True, False, 1, map_1_dither_notable_1,   MAG_1_dither_notable_1},
    {  True, True, True, 8, map_1_dither_table_8,      MAG_1_dither_table_8},
    {  True, False,True, 8, map_1_nodither_table_8,    MAG_1_nodither_table_8},
    {  True, False,False,32,map_1_nodither_notable_32, MAG_1_nodither_notable_32},
    {  False,True, True, 8, map_2or3_dither_table_8,   MAG_2or3_dither_table_8},
    {  False,False,True, 8, map_2or3_nodither_table_8, MAG_2or3_nodither_table_8},
    {  False,True, False,32,map_2or3_dither_notable_32,MAG_2or3_dither_notable_32},
    {  False,False,False,32,map_2or3_nodither_notable_32,MAG_2or3_nodither_notable_32}};

    img->map_scanline = map_scanline_generic;
    img->MAG_scanline = MAG_scanline_generic;

    if (img->pixel_table == NULL)
	table_present = False;
    else table_present = True;

    if (img->mono_color)
	switch (img->image->bits_per_pixel) {
	case 32:
	    img->map_scanline = map_1_mono_color_32;
	    img->MAG_scanline = MAG_1_mono_color_32;
	    break;
	case 8:
	    img->map_scanline = map_1_mono_color_8;
	    img->MAG_scanline = MAG_1_mono_color_8;
	    break;
	default:
	    fprintf(stderr, "%s: Warning: Bits per pixel = %d\n",
		    progname, img->image->bits_per_pixel);
	}
    else
	for (i = 0; i < COUNT_OF (map_scanline_table); i++) {
	    if (map_scanline_table[i].one_color == img->mono_img &&
		map_scanline_table[i].dither == img->dither_img &&
		map_scanline_table[i].table_present == table_present &&
		map_scanline_table[i].bpp == img->image->bits_per_pixel) {
		img->map_scanline = map_scanline_table[i].routine;
		img->MAG_scanline = map_scanline_table[i].mag_routine;
		
		if ( verbose_flag ) {
		    fprintf ( stderr, "Special map_scanline routine used: map_");
		    
		    if ( img->mono_img )
			fputs ( "1_", stderr );
		    else fputs ( "2or3_", stderr );
		    
		    if ( !img->dither_img )
			fputs ( "no", stderr );
		    fputs ( "dither_", stderr );
		    
		    if (! table_present ) fputs ( "no", stderr );
		    fprintf ( stderr, "table_%d (index %d)\n",
			      img->image->bits_per_pixel, i );
		}
		break;
	    }
	}
}

/*
 * Map a scanline through the dither matrix.
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */

#define DMAP(v,x,y)	(divN[v] + (modN[v]>dm16[x][y] ? 1 : 0) )
#define NDMAP(v)	(divN[v])

#define DMAP_SETUP( img ) \
    register int *divN = img->divN;\
    register int *modN = img->modN;\
    register array16 *dm16 = img->dm16

#define NDMAP_SETUP( img ) register int *divN = img->divN

#define IN_CMAP_SETUP( img ) register rle_pixel **in_cmap = img->in_cmap

#define LEVELS_SETUP( img ) \
    register int levels = img->lvls; \
    register int levels_squared = img->lvls_squared

/* Note! This must go at the end of the declarations section. */
#define X_CMAP_SETUP( img ) cmap_info cmap_i; cmap_i = img->x_cmap;


static void
map_scanline_generic (img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
int		stride;
int		y;
XImage		*image;
{
    register Pixel *pixel_table = img->pixel_table;
    DMAP_SETUP( img );
    LEVELS_SETUP( img );
    register int x;
    register int col;
    register int row;
    register unsigned char *r;
    register long	pixel;
    register int	width = given_width;
    X_CMAP_SETUP( img );
    
    row = y % 16;
    col = 0;
    r = rgb[0];
    
    if (ncolors == 1) {
	
	if ( img->dither_img ) {
	    for (x = 0; 
		 --width >= 0; 
		 r += stride, col = ((col + 1) & 15), x ++) {
		pixel = DMAP(*r, col, row);
		pixel = ((pixel_table != NULL) ? pixel_table[pixel] : 
			 SHIFT_MASK_PIXEL( pixel, pixel, pixel ));
		XPutPixel (image, x, y, pixel);
	    }
	}
	
	else {
	    if (img->mono_color) 
		for (x = 0; --width >= 0; 
		     r += stride, x ++) {
		    pixel = pixel_table[*r];
		    XPutPixel (image, x, y, pixel);
		}
	    else
		for (x = 0; --width >= 0; 
		     r += stride, x ++) {
		    pixel = NDMAP(*r);
		    pixel = ((pixel_table != NULL) ? pixel_table[pixel] : 
			     SHIFT_MASK_PIXEL( pixel, pixel, pixel ));
		    XPutPixel (image, x, y, pixel);
		}
	}
	
	
    }
    
    else {
	register unsigned char *g;
	register unsigned char *b;
	
	g = b = rgb[1]; 
	if (ncolors >= 3) b = rgb[2];
	
	if ( img->dither_img ) {
	    for (x = 0; --width >= 0; 
		 r += stride, g += stride, b += stride, 
		 col = ((col + 1) & 15), x ++ )
	    {
		if (pixel_table != NULL) 
		    pixel = pixel_table
			[DMAP(*r, col, row) +
			 DMAP(*g, col, row) * levels +
			 DMAP(*b, col, row) * levels_squared];

		else pixel = SHIFT_MASK_PIXEL(DMAP(*r, col, row),
					      DMAP(*g, col, row),
					      DMAP(*b, col, row));

		XPutPixel (image, x, y, pixel);
	    }
	    
	}
	
	else {
	    
	    for (x = 0; --width >= 0; 
		 r += stride, g += stride, b += stride, x++ ) {
		if ( pixel_table != NULL) {
		    pixel = pixel_table
			[NDMAP(*r) +
			 NDMAP(*g) * levels +
			 NDMAP(*b) * levels_squared];
		}
		else {
		    pixel = SHIFT_MASK_PIXEL (NDMAP(*r), NDMAP(*g), NDMAP(*b));
		}
		XPutPixel (image, x, y, pixel);
	    }
	}
    }	
}

static void
MAG_scanline_generic( img, rle_x, rle_y, mag_size, x, y, width, height, image )
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;

{
    register Pixel *pixel_table = img->pixel_table;
    DMAP_SETUP( img );
    LEVELS_SETUP( img );
    register int 	col;
    register unsigned char *r;
    int			save_x = x;
    register int 	mag_x;
    int 		mag_y = mag_size;
    register int 	w;
    register int 	row = (mag_size * rle_y - 1) % 16;
    int 		x_mod_16 = (mag_size * rle_x) % 16;
    unsigned char	*rgb_line = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    int			rgb_line_stride = img->w * img->dpy_channels;
    register unsigned long pixel;
    X_CMAP_SETUP( img );

    while (--height >= 0) {
	x = save_x;
	w = width;
	row = (row + 1) % 16;
	col = x_mod_16;
	r = rgb_line;
	mag_x = mag_size;
	
	if ( img->dpy_channels == 1 ) {
	    
	    if ( img->dither_img )
		while (--w >= 0) 
		{
		    pixel = DMAP(*r, col++, row);
		    pixel = ((pixel_table != NULL) ? pixel_table[pixel] : 
			     SHIFT_MASK_PIXEL( pixel, pixel, pixel ));
		    XPutPixel (image, x, y, pixel);
		    if ( --mag_x == 0 )
		    {
			r++; 
			mag_x = mag_size;
		    }
		    col &=15; x++; 
		}
	    else 
		if (img->mono_color) 
		    while (--w >= 0)
		    {
			pixel = pixel_table[*r];
			XPutPixel (image, x, y, pixel);
			if ( --mag_x == 0 )
			{
			    r++; 
			    mag_x = mag_size;
			}
			x++;		    
		    }
		else
		    while (--w >= 0)
		    {
			pixel = NDMAP(*r);
			pixel = ((pixel_table != NULL) ? pixel_table[pixel] : 
				 SHIFT_MASK_PIXEL( pixel, pixel, pixel ));
			XPutPixel (image, x, y, pixel);
			if ( --mag_x == 0 )
			{
			    r++; 
			    mag_x = mag_size;
			}
			x++; 
		    }
	}
	
	else {
	    register unsigned char *g;
	    register unsigned char *b;
	    
	    g = b = r + img->w; 
	    if (img->dpy_channels >= 3) b += img->w;
	    
	    if ( img->dither_img ) 
		while (--w >= 0) 
		{
		    if ( pixel_table != NULL) 
			pixel = pixel_table
			    [DMAP(*r, col, row) +
			     DMAP(*g, col, row) * levels +
			     DMAP(*b, col, row) * levels_squared];
		    
		    else pixel =
			SHIFT_MASK_PIXEL (DMAP(*r, col, row),
					  DMAP(*g, col, row),
					  DMAP(*b, col, row));
		    XPutPixel (image, x++, y, pixel);
		    if ( --mag_x == 0 )
		    {
			r++, g++, b++; 
			mag_x = mag_size;
		    }
		    col++;
		    col &= 15;
		}
	    else 
		while (--w >= 0) 
		{
		    if ( pixel_table != NULL) 
			pixel = pixel_table
			    [NDMAP(*r) +
			     NDMAP(*g) * levels +
			     NDMAP(*b) * levels_squared];
		    else pixel = SHIFT_MASK_PIXEL (NDMAP(*r),
						   NDMAP(*g),
						   NDMAP(*b));
		
		    XPutPixel (image, x++, y, pixel);
		    if ( --mag_x == 0 )
		    {
			r++, g++, b++; 
			mag_x = mag_size;
		    }
		}
	}
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
	y++;
    }	
}

/*
 * map_1_dither_table_8
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */

static
void
map_1_dither_table_8(img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
int		stride;
int		y;
XImage		*image;
{
    register Pixel *pixel_table = img->pixel_table;
    DMAP_SETUP( img );
    register int col;
    register int row;
    register unsigned char *r;
    register unsigned char *pixel_ptr;
    register int width = given_width;
    
    row = y % 16;
    col = 0;
    r = rgb[0];
    
    pixel_ptr = ((unsigned char *) image->data) + y * image->bytes_per_line;
    
    while (--width >= 0) {
	*pixel_ptr++ = pixel_table [ DMAP(*r, col, row) ];
	
	r += stride;
	col = ((col + 1) & 15);
    }
}

static
void
MAG_1_dither_table_8( img, rle_x, rle_y, mag_size, x, y, width, height, image )
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;
{
    register Pixel *pixel_table = img->pixel_table;
    DMAP_SETUP( img );
    register int 	col;
    register unsigned char *r;
    register unsigned char *pixel_ptr;
    register int 	mag_x;
    int 		mag_y = mag_size;
    register int 	w;
    register int 	row = (mag_size * rle_y - 1) % 16;
    int 		x_mod_16 = (mag_size * rle_x) % 16;
    unsigned char 	*line_ptr;
    unsigned char	*rgb_line = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    int			rgb_line_stride = img->w * img->dpy_channels;
    
    line_ptr = ((unsigned char *) image->data)+(y * image->bytes_per_line) + x;

    while (--height >= 0) {
	pixel_ptr = line_ptr;
	w = width;
	row = (row + 1) % 16;
	col = x_mod_16;
	r = rgb_line;
	mag_x = mag_size;
	
	while (--w >= 0) {
	    *pixel_ptr++ = pixel_table [DMAP(*r, col++, row) ];
	    col &= 15;
	    if ( --mag_x == 0 )
	    {
		r++;
		mag_x = mag_size;
	    }
	}
	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}

/*
 * map_2or3_dither_table_8
 *
 * Dither three colors into an eight bit table...  This is faster than the
 * map_scanline_generic routine.
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */

static
void
map_2or3_dither_table_8 (img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
int		stride;
int		y;
XImage		*image;
    
{
    register Pixel *pixel_table = img->pixel_table;
    DMAP_SETUP( img );
    LEVELS_SETUP( img );
    register int col;
    register int row;
    register unsigned char *r, *g, *b;
    register unsigned char *pixel_ptr;
    register int width = given_width;
    
    row = y % 16;
    col = 0;
    r = rgb[0];
    g = b = rgb[1]; 
    if (ncolors >= 3) b = rgb[2];

    pixel_ptr = ((unsigned char *) image->data) + y * image->bytes_per_line;
    
    while (--width >= 0) {
	*pixel_ptr++ = pixel_table [DMAP(*r, col, row) +
				    DMAP(*g, col, row) * levels +
				    DMAP(*b, col, row) * levels_squared];
	r += stride;
	g += stride;
	b += stride;
	col = ((col + 1) & 15);
    }
}
static
void
MAG_2or3_dither_table_8 ( img, rle_x, rle_y, mag_size, x, y, width, height, image)
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;
    
{
    register Pixel *pixel_table = img->pixel_table;
    DMAP_SETUP( img );
    LEVELS_SETUP( img );
    register int col;
    register unsigned char *r, *g, *b;
    register unsigned char *pixel_ptr;
    register int 	mag_x;
    int 		mag_y = mag_size;
    register int 	w;
    register int 	row = (mag_size * rle_y - 1) % 16;
    int 		x_mod_16 = (mag_size * rle_x) % 16;
    unsigned char 	*line_ptr;
    unsigned char	*rgb_line = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    int			rgb_line_stride = img->w * img->dpy_channels;
    
    line_ptr = ((unsigned char *) image->data)+(y * image->bytes_per_line) + x;

    while (--height >= 0) {
	pixel_ptr = line_ptr;
	w = width;
	row = (row + 1) % 16;
	col = x_mod_16;
	mag_x = mag_size;

	r = rgb_line;
	g = b = r + img->w; 
	if (img->dpy_channels >= 3) b += img->w;
	
	while (--w >= 0) {
	    *pixel_ptr++ = pixel_table [DMAP(*r, col, row) +
					DMAP(*g, col, row) * levels +
					DMAP(*b, col, row) * levels_squared];
	    col++;
	    col &= 15;
	    if ( --mag_x == 0 )
	    {
		r++, g++, b++;
		mag_x = mag_size;
	    }
	}
	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}

/*
 * map_1_dither_notable_1
 *
 * Dither a 1 channel image into a 1 bit image.
 * 
 * Inputs:
 * 	rgb:		Pointer to buffer containing gray scale information.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */
static
void
map_1_dither_notable_1 (img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
register int	stride;
int		y;
XImage		*image;
    
{
    DMAP_SETUP( img );
    register int col;
    register int row;
    register unsigned char *r;
    register unsigned char *pixel_ptr;
    register int width = given_width;
    int bit;
    row = y % 16;
    col = bit = 0;
    r = rgb[0];

    pixel_ptr = ((unsigned char *) image->data) + y * image->bytes_per_line;

    if( BitmapBitOrder(dpy) == MSBFirst ) {
	    /* we don't want to trash those good bits */
	    *pixel_ptr >>= (8 - bit) & 7;

	    /* do first byte fragment */
	    while ( col & 7 )
	    {
		*pixel_ptr <<= 1;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x1;
		width--;
		r += stride;
	    }
	    if ( bit )

		pixel_ptr++, col &= 15;
	    
	    while ((width -= 8) >= 0) {
		*pixel_ptr = 0;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr = (unsigned char) 0x80;
		r += stride;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x40;
		r += stride;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x20;
		r += stride;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x10;
		r += stride;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x8;
		r += stride;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x4;
		r += stride;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x2;
		r += stride;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x1;
		r += stride;
		col &= 15;
		pixel_ptr++;
	    }
	    
	    /* if w is non-zero then we have to finish up... */
	    width = 8 + width;
	    if ( width )
	    {
		unsigned char savebits;

		savebits = *pixel_ptr & LSBMask[8-width];
		bit = width;
		
		while (--width >= 0) {
		    *pixel_ptr <<= 1;
		    if ( DMAP(*r, col++, row) )
			*pixel_ptr |= (unsigned char) 0x1;
		    r += stride;
		}
		*pixel_ptr <<= 8 - bit;
		*pixel_ptr |= savebits;
	    }
	}
    else
	while (--width >= 0) {
	    *pixel_ptr = (unsigned char)(*pixel_ptr >> 1) | (unsigned char)
		((DMAP(*r, col, row)!= 0) ? 0x80: 0);
	    
	    r += stride;
	    col = ((col + 1) & 15);
	    bit = ((bit + 1) & 7);
	    if (!bit) pixel_ptr++;
	}
}
#define INC_RGB( stmt ) if ( --mag_x == 0 ) {stmt; mag_x = mag_size; }
static
void
MAG_1_dither_notable_1 ( img, rle_x, rle_y, mag_size, x, y, width, height, image )
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;
{
    DMAP_SETUP( img );
    register int col, bit;
    register unsigned char *r;
    register unsigned char *pixel_ptr;
    register int      mag_x;
    register int      w;
    register int      row = (mag_size * rle_y - 1) % 16;
    unsigned char     *line_ptr;
    unsigned char     *rgb_line = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    int		      rgb_line_stride = img->w * img->dpy_channels;
    int 	      x_mod_16 = (mag_size * rle_x) % 16;
    int 	      x_mod_8 = x % 8;
    int 	      mag_y = mag_size;
    int 	      byte_order = BitmapBitOrder(dpy);

    line_ptr = ((unsigned char *) image->data) +
	(y * image->bytes_per_line) + x / 8;

    while (--height >= 0) {
	pixel_ptr = line_ptr;
	w = width;
	r = rgb_line;
	mag_x = mag_size;

	row = (row + 1) % 16;
	col = x_mod_16;
	bit = x_mod_8;

	if( byte_order == MSBFirst ) {
	    /* we don't want to trash those good bits */
	    *pixel_ptr >>= (8 - bit) & 7;

	    /* do first byte fragment */
	    while ( bit & 7 )
	    {
		*pixel_ptr <<= 1;
		*pixel_ptr |= DMAP(*r, col++, row)!=0;
		w--;
		INC_RGB( r++ );
		bit++;
		col &= 15;
	    }
	    if ( x_mod_8 )
		pixel_ptr++;

	    /* do the bulk of the line fast in eight bit chunks Gee I hope all
	     * this fits into your instruction cache...  Or else we are
	     * forked..   You can get rid of 7 (col &= 15)'s if you make dm16
	     * a 32x16 array with duplicates in the second half of the columns.
	     * Then we don't have to worry about col overflowing in 8 ++'s
	     */
	    while ((w -= 8) >= 0) {
		*pixel_ptr = (unsigned char) 0;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr = (unsigned char) 0x80;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x40;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x20;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x10;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x8;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x4;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x2;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x1;
		INC_RGB( r++ ); col &= 15;
		pixel_ptr++;
	    }

	    /* if w is non-zero then we have to finish up... */
	    w = 8 + w;
	    if ( w )
	    {
		unsigned char savebits;

		savebits = *pixel_ptr & LSBMask[8-w];
		bit = w;
		
		while (--w >= 0) {
		    *pixel_ptr <<= 1;
		    *pixel_ptr |= (unsigned char)
			(DMAP(*r, col++, row) ? 0x1 : 0);
		    INC_RGB( r++ ); col &= 15;
		}
		*pixel_ptr <<= 8 - bit;
		*pixel_ptr |= savebits;
	    }
	}
	else {
	    /* we don't want to trash those good bits */
	    *pixel_ptr <<= (8 - bit) & 7;

	    /* do first byte fragment */
	    while ( col & 7 ) {
		*pixel_ptr >>= 1;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x80;
		w--;
		INC_RGB( r++ ); col &= 15;
	    }
	    if ( x_mod_8 )
		pixel_ptr++;

	    /* do the bulk of the line fast in eight bit chunks.. */
	    while ((w -= 8) >= 0) {
		*pixel_ptr = (unsigned char) 0x0;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr = (unsigned char) 0x1;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x2;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x4;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x8;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x10;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x20;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x40;
		INC_RGB( r++ ); col &= 15;
		if ( DMAP(*r, col++, row) )
		    *pixel_ptr |= (unsigned char) 0x80;
		INC_RGB( r++ ); col &= 15;
		pixel_ptr++;
	    }

	    /* if w is negative then we have to finish up... */
	    w = 0 - w;
	    
	    if ( w )
	    {
		unsigned char savebits = (unsigned char )(*pixel_ptr >> 8 - w);
		savebits <<= (unsigned char)8 - w;
		bit = w;
		
		while (--w >= 0) {
		    *pixel_ptr >>= 1;
		    if (DMAP(*r, col++, row))
			*pixel_ptr |= (unsigned char) 0x80;
		    INC_RGB( r++ ); col &= 15;
		}
		*pixel_ptr >>= 8 - bit;
		*pixel_ptr |= savebits;
	    }
	}
	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}

/*
 * map_2or3_nodither_table_8
 *
 * Dither three colors into an eight bit table...  This is faster than the
 * map_scanline_generic routine.
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */

static
void
map_2or3_nodither_table_8 (img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
int		stride;
int		y;
XImage		*image;
    
{
    register Pixel *pixel_table = img->pixel_table;
    NDMAP_SETUP( img );
    LEVELS_SETUP( img );
    register unsigned char *r, *g, *b;
    register unsigned char *pixel_ptr;
    register int width = given_width;
    
    r = rgb[0];
    g = b = rgb[1]; 
    if (ncolors >= 3) b = rgb[2];

    pixel_ptr = ((unsigned char *) image->data) + y * image->bytes_per_line;
    
    while (--width >= 0) {
	*pixel_ptr++ = pixel_table [  NDMAP(*r)
				    + NDMAP(*g) * levels
				    + NDMAP(*b) * levels_squared];
	r += stride;
	g += stride;
	b += stride;
    }
}
static
void
MAG_2or3_nodither_table_8( img, rle_x, rle_y, mag_size, x, y, width, height, image )
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;
   
{
    register Pixel *pixel_table = img->pixel_table;
    NDMAP_SETUP( img );
    LEVELS_SETUP( img );
    register unsigned char *r, *g, *b;
    register unsigned char *pixel_ptr;
    register unsigned char table_value;
    register int 	mag_x;
    int 		mag_y = mag_size;
    register int 	w;
    unsigned char 	*line_ptr, *last_line;
    unsigned char	*rgb_line, *last_rgb;
    int			rgb_line_stride = img->w * img->dpy_channels;

    rgb_line = last_rgb = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    
    last_line = line_ptr =
	((unsigned char *) image->data)+(y * image->bytes_per_line) + x;
    
    while (--height >= 0) {
	if ( rgb_line == last_rgb && last_line != line_ptr )
	    memcpy( line_ptr, last_line, width );
	else
	{
	    pixel_ptr = line_ptr;
	    w = width;
	    r = rgb_line;
	    mag_x = mag_size;
	    
	    g = b = r + img->w; 
	    if (img->dpy_channels >= 3) b += img->w;
	    
	    table_value = pixel_table [  NDMAP(*r)
				       + NDMAP(*g) * levels
				       + NDMAP(*b) * levels_squared];
	    while ( --w >= 0 ) {
		*pixel_ptr++ = table_value;
		if ( --mag_x == 0 )
		{
		    r++, g++, b++;
		    mag_x = mag_size;
		    table_value =
			pixel_table [  NDMAP(*r)
				     + NDMAP(*g) * levels
				     + NDMAP(*b) * levels_squared];
		}
	    }
	}
	last_line = line_ptr;
	last_rgb = rgb_line;

	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}

/*
 * map_1_nodither_table_8
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */

static void
map_1_nodither_table_8 (img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
int		stride;
int		y;
XImage		*image;

{
    register Pixel *pixel_table = img->pixel_table;
    NDMAP_SETUP( img );
    register unsigned char 	* r;
    register unsigned char	* pixel_ptr;
    register int	width = given_width;
    
    r = rgb[0];
    
    pixel_ptr = ((unsigned char *) image->data) + y * image->bytes_per_line;
    
    while (--width >= 0) {
	*pixel_ptr++ = pixel_table [ NDMAP(*r) ];
	r += stride;
    }
    
}
static void
MAG_1_nodither_table_8(img, rle_x, rle_y, mag_size, x, y, width, height, image)
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;
{
    register Pixel *pixel_table = img->pixel_table;
    NDMAP_SETUP( img );
    register unsigned char 	* r;
    register unsigned char	* pixel_ptr;
    register unsigned char	table_value;
    register int 	mag_x;
    int 		mag_y = mag_size;
    register int 	w;
    unsigned char 	*line_ptr, *last_line;
    unsigned char	*rgb_line, *last_rgb;
    int			rgb_line_stride = img->w * img->dpy_channels;
    
    rgb_line = last_rgb = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    
    last_line = line_ptr =
	((unsigned char *) image->data)+(y * image->bytes_per_line) + x;

    while (--height >= 0) {
	if ( rgb_line == last_rgb && last_line != line_ptr )
	    memcpy( line_ptr, last_line, width );
	else
	{
	    pixel_ptr = line_ptr;
	    w = width;
	    r = rgb_line;
	    mag_x = mag_size;
	    
	    table_value = pixel_table [ NDMAP(*r) ];
	    while (--w >= 0) {
		*pixel_ptr++ = table_value;
		if ( --mag_x == 0 ){
		    r++;
		    mag_x = mag_size;
		    table_value = pixel_table [ NDMAP(*r) ];
		}
	    }
	}

	last_line = line_ptr;
	last_rgb = rgb_line;

	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}
/*
 * map_1_mono_color_8
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */

static void
map_1_mono_color_8 (img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
int		stride;
int		y;
XImage		*image;

{
    register Pixel *pixel_table = img->pixel_table;
    register unsigned char 	* r;
    register unsigned char	* pixel_ptr;
    register int width = given_width;
    
    r = rgb[0];
    
    pixel_ptr = ((unsigned char *) image->data) + y * image->bytes_per_line;
    
    while (--width >= 0) {
	*pixel_ptr++ = pixel_table [ *r ];
	r += stride;
    }
    
}
static void
MAG_1_mono_color_8 (img, rle_x, rle_y, mag_size, x, y, width, height, image )
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;
{
    register Pixel *pixel_table = img->pixel_table;
    register unsigned char 	* r;
    register unsigned char	* pixel_ptr;
    register unsigned char	table_value;
    register int 	mag_x;
    int 		mag_y = mag_size;
    register int 	w;
    unsigned char 	*line_ptr, *last_line;
    unsigned char	*rgb_line, *last_rgb;
    int			rgb_line_stride = img->w * img->dpy_channels;
    
    rgb_line = last_rgb = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    
    last_line = line_ptr =
	((unsigned char *) image->data)+(y * image->bytes_per_line) + x;

    while (--height >= 0) {
	if ( rgb_line == last_rgb && last_line != line_ptr )
	    memcpy( line_ptr, last_line, width );
	else {
	    pixel_ptr = line_ptr;
	    w = width;
	    r = rgb_line;
	    mag_x = mag_size;
	    
	    table_value = pixel_table [ *r ];
	    while (--w >= 0) {
		*pixel_ptr++ = table_value;
		if ( --mag_x == 0 ){
		    r++;
		    mag_x = mag_size;
		    table_value = pixel_table [ *r ];
		}
	    }
	}

	last_line = line_ptr;
	last_rgb = rgb_line;

	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}

/*
 * map_2or3_dither_table_32
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */

#if 0				/* Never called. */
static
void
map_2or3_dither_table_32 ( img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
int		stride;
int		y;
XImage		*image;

{
    register Pixel *pixel_table = img->pixel_table;
    DMAP_SETUP( img );
    LEVELS_SETUP( img );
    register int col;
    register int row;
    register unsigned char * r;
    register unsigned char *g;
    register unsigned char *b;
    register unsigned long pixval;
    register unsigned char * pixel_ptr;
    register int width = given_width;
    int byte_order = ImageByteOrder( dpy );
    
    row = y % 16;
    col = 0;
    r = rgb[0];
    
    g = b = rgb[1]; 
    if (ncolors >= 3) b = rgb[2];
    
    pixel_ptr = (unsigned char *)image->data + y * image->bytes_per_line;
    
    while (--width >= 0) {
	
	pixval = pixel_table [DMAP(*r, col, row) +
				    DMAP(*g, col, row) * levels +
				    DMAP(*b, col, row) * levels_squared];
	if ( byte_order == MSBFirst )
	{
	    pixel_ptr += 3;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr = pixval & 0xff;
	    pixel_ptr += 4;
	}
	else
	{
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	}
	r += stride;
	g += stride;
	b += stride;
	col = ((col + 1) & 15);
	
    }
    
}
#endif

#if 0				/* Never called. */
static
void
MAG_2or3_dither_table_32 ( img, rle_x, rle_y, mag_size, x, y, width, height, image )
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;
{
    register Pixel *pixel_table = img->pixel_table;
    DMAP_SETUP( img );
    LEVELS_SETUP( img );
    register int 	col;
    register unsigned long pixval;
    register unsigned char * pixel_ptr;
    register unsigned char *r;
    register unsigned char *g;
    register unsigned char *b;
    register int 	w;
    register int 	row = (mag_size * rle_y - 1) % 16;
    register int 	mag_x;
    int 		mag_y = mag_size;
    int 		x_mod_16 = (mag_size * rle_x) % 16;
    unsigned char 	*line_ptr;
    unsigned char	*rgb_line = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    int			rgb_line_stride = img->w * img->dpy_channels;
    int byte_order = ImageByteOrder( dpy );
    
    line_ptr = (unsigned char *)
	((image->data) + (y * image->bytes_per_line)) + 4 * x;

    while (--height >= 0) {
	pixel_ptr = line_ptr;
	w = width;
	row = (row + 1) % 16;
	col = x_mod_16;
	r = rgb_line;
	mag_x = mag_size;

	g = b = r + img->w; 
	if (img->dpy_channels >= 3) b += img->w;
	    
	while (--w >= 0) {
	    pixval = pixel_table [DMAP(*r, col, row) +
					DMAP(*g, col, row) * levels +
					DMAP(*b, col, row) * levels_squared];
	    if ( byte_order == MSBFirst )
	    {
		pixel_ptr += 3;
		*pixel_ptr-- = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr-- = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr-- = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr = pixval & 0xff;
		pixel_ptr += 4;
	    }
	    else
	    {
		*pixel_ptr++ = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr++ = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr++ = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr++ = pixval & 0xff;
	    }
	    if ( --mag_x == 0 )
	    {
		r++, g++, b++; 
		mag_x = mag_size;
	    }
	    col++;
	    col &= 15;
	}
	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}
#endif

/*
 * map_2or3_dither_notable_32
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */
static void
map_2or3_dither_notable_32 (img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
int		stride;
int		y;
XImage		*image;

{
    DMAP_SETUP( img );
    register int	col;
    register int	row;
    register unsigned char *r;
    register unsigned char *g;
    register unsigned char *b;
    register unsigned long pixval;
    register unsigned char *pixel_ptr;
    register int	width = given_width;
    int byte_order = ImageByteOrder( dpy );
    X_CMAP_SETUP( img );
    
    row = y % 16;
    col = 0;
    r = rgb[0];
    g = rgb[1]; 
    if (ncolors >= 3) b = rgb[2];
    else b = rgb[1];
    
    pixel_ptr = (unsigned char *) ( image->data + y * image->bytes_per_line );
    
    while (--width >= 0) {
	
	pixval = SHIFT_MASK_PIXEL (DMAP(*r, col, row),
				   DMAP(*g, col, row),
				   DMAP(*b, col, row));
	if ( byte_order == MSBFirst )
	{
	    pixel_ptr += 3;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr = pixval & 0xff;
	    pixel_ptr += 4;
	}
	else
	{
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	}
	r += stride;
	g += stride;
	b += stride;
	col = ((col + 1) & 15);
    }
}
static void
MAG_2or3_dither_notable_32 ( img, rle_x, rle_y, mag_size, x, y, width, height, image )
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;
{
    DMAP_SETUP( img );
    register unsigned char *r, *g, *b;
    register unsigned long pixval;
    register unsigned char *pixel_ptr;
    register int	row = (mag_size * rle_y - 1) % 16;
    register int	col;
    register int 	mag_x;
    int 		mag_y = mag_size;
    register int 	w;
    int 		x_mod_16 = (mag_size * rle_x) % 16;
    unsigned char 	*line_ptr;
    unsigned char	*rgb_line = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    int			rgb_line_stride = img->w * img->dpy_channels;
    int			byte_order = ImageByteOrder( dpy );
    X_CMAP_SETUP( img );

    line_ptr = (unsigned char *)
	(image->data + (y * image->bytes_per_line)) + 4 * x;

    while (--height >= 0) {
	pixel_ptr = line_ptr;
	w = width;
	row = (row + 1) % 16;
	col = x_mod_16;
	r = rgb_line;
	mag_x = mag_size;
	
	g = b = r + img->w; 
	if (img->dpy_channels >= 3) b += img->w;
	
	while (--w >= 0) {
	    pixval = SHIFT_MASK_PIXEL(DMAP(*r, col, row),
				      DMAP(*g, col, row),
				      DMAP(*b, col, row));
	    if ( byte_order == MSBFirst )
	    {
		pixel_ptr += 3;
		*pixel_ptr-- = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr-- = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr-- = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr = pixval & 0xff;
		pixel_ptr += 4;
	    }
	    else
	    {
		*pixel_ptr++ = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr++ = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr++ = pixval & 0xff;
		pixval >>= 8;
		*pixel_ptr++ = pixval & 0xff;
	    }
	    if ( --mag_x == 0 )
	    {
		r++, g++, b++; 
		mag_x = mag_size;
	    }
	    col++;
	    col &= 15;
	}
	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}


/*
 * map_2or3_nodither_notable_32
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */

static
void
map_2or3_nodither_notable_32 (img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
int		stride;
int		y;
XImage		*image;

{
    NDMAP_SETUP( img );
    register unsigned char *r;
    register unsigned char *g;
    register unsigned char *b;
    register unsigned long pixval;
    register unsigned char *pixel_ptr;
    register int width = given_width;
    int byte_order = ImageByteOrder( dpy );
    X_CMAP_SETUP( img );
    
    r = rgb[0];
    g = rgb[1]; 
    if (ncolors >= 3) b = rgb[2];
    else b = rgb[1];

    pixel_ptr = (unsigned char *) ( image->data + y * image->bytes_per_line );
    
    while (--width >= 0) {
	
	pixval = SHIFT_MASK_PIXEL_32(NDMAP(*r), NDMAP(*g), NDMAP(*b));
	if ( byte_order == MSBFirst )
	{
	    pixel_ptr += 3;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr = pixval & 0xff;
	    pixel_ptr += 4;
	}
	else
	{
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	}

	r += stride;
	g += stride;
	b += stride;
	
    }
    
}
static
void
MAG_2or3_nodither_notable_32 ( img, rle_x, rle_y, mag_size, x, y, width, height, image )
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;

{
    NDMAP_SETUP( img );
    register unsigned char *r, *g, *b;
    register unsigned long pixval;
    register unsigned char *pixel_ptr;
    unsigned long pixel_value;
    register int 	mag_x;
    int 		mag_y = mag_size;
    register int 	w;
    unsigned char 	*line_ptr, *last_line;
    unsigned char	*rgb_line, *last_rgb; 
    int			rgb_line_stride = img->w * img->dpy_channels;
    int byte_order = ImageByteOrder( dpy );
    X_CMAP_SETUP( img );

    rgb_line = last_rgb = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    
    last_line = line_ptr = (unsigned char *)
	(image->data + (y * image->bytes_per_line)) + 4 * x;

    while (--height >= 0) {
	if ( rgb_line == last_rgb && last_line != line_ptr )
	    memcpy( line_ptr, last_line, width * sizeof(long) );
	else
	{
	    pixel_ptr = line_ptr;
	    w = width;
	    r = rgb_line;
	    mag_x = mag_size;
	    
	    g = b = r + img->w; 
	    if (img->dpy_channels >= 3) b += img->w;
	    
	    pixel_value = SHIFT_MASK_PIXEL_32(NDMAP(*r), NDMAP(*g), NDMAP(*b));
    
	    while (--w >= 0) {
		pixval = pixel_value;
		if ( byte_order == MSBFirst )
		{
		    pixel_ptr += 3;
		    *pixel_ptr-- = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr-- = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr-- = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr = pixval & 0xff;
		    pixel_ptr += 4;
		}
		else
		{
		    *pixel_ptr++ = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr++ = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr++ = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr++ = pixval & 0xff;
		}
		if ( --mag_x == 0 )
		{
		    r++, g++, b++; 
		    mag_x = mag_size;
		    pixel_value = SHIFT_MASK_PIXEL_32(NDMAP(*r),
						      NDMAP(*g), NDMAP(*b));
		}
	    }
	}
	last_line = line_ptr;
	last_rgb = rgb_line;

	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}

/*
 * map_1_nodither_notable_32
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */

static
void
map_1_nodither_notable_32 (img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
register int	stride;
int		y;
XImage		*image;

{
    NDMAP_SETUP( img );
    register unsigned char *r;
    register unsigned long pixval;
    register unsigned char *pixel_ptr;
    register int width = given_width;
    int byte_order = ImageByteOrder( dpy );
    X_CMAP_SETUP( img );
    
    r = rgb[0];

    pixel_ptr = (unsigned char *) ( image->data + y * image->bytes_per_line );
    
    while (--width >= 0) {
	register int bw_value = NDMAP(*r);
	
	pixval = SHIFT_MASK_PIXEL_32( bw_value, bw_value, bw_value );
	if ( byte_order == MSBFirst )
	{
	    pixel_ptr += 3;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr = pixval & 0xff;
	    pixel_ptr += 4;
	}
	else
	{
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	}

	r += stride;
    }
    
}
static
void
MAG_1_nodither_notable_32 ( img, rle_x, rle_y, mag_size, x, y, width, height, image )
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;

{
    NDMAP_SETUP( img );
    register unsigned char *r;
    register unsigned long pixval;
    register unsigned char *pixel_ptr;
    unsigned long pixel_value;
    register int bw_value;
    register int 	mag_x;
    register int 	w;
    register int 	mag_y = mag_size;
    unsigned char 	*line_ptr, *last_line;
    unsigned char	*rgb_line, *last_rgb;
    int			rgb_line_stride = img->w * img->dpy_channels;
    int byte_order = ImageByteOrder( dpy );
    X_CMAP_SETUP( img );

    rgb_line = last_rgb = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    
    last_line = line_ptr = (unsigned char *)
	(image->data + (y * image->bytes_per_line)) + 4 * x;

    while (--height >= 0) {
	if ( rgb_line == last_rgb && last_line != line_ptr )
	    memcpy( line_ptr, last_line, width * sizeof( long ) );
	else
	{
	    pixel_ptr = line_ptr;
	    w = width;
	    r = rgb_line;
	    mag_x = mag_size;
	    
	    bw_value = NDMAP(*r);
	    pixel_value = SHIFT_MASK_PIXEL_32(bw_value, bw_value, bw_value );
    
	    while (--w >= 0) {
		pixval = pixel_value;
		if ( byte_order == MSBFirst )
		{
		    pixel_ptr += 3;
		    *pixel_ptr-- = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr-- = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr-- = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr = pixval & 0xff;
		    pixel_ptr += 4;
		}
		else
		{
		    *pixel_ptr++ = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr++ = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr++ = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr++ = pixval & 0xff;
		}
		if ( --mag_x == 0 )
		{
		    r++; 
		    mag_x = mag_size;
		    
		    bw_value = NDMAP(*r);
		    pixel_value =
			SHIFT_MASK_PIXEL_32(bw_value, bw_value, bw_value );
		}
	    }
	}
	last_line = line_ptr;
	last_rgb = rgb_line;

	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}
/*
 * map_1_mono_color_32
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */

static
void
map_1_mono_color_32 (img, rgb, ncolors, given_width, stride, y, image)
image_information *img;
unsigned char	*rgb[3];
int		ncolors;
int		given_width;
register int	stride;
int		y;
XImage		*image;
{
    register unsigned char *r;
    register unsigned long pixval;
    register unsigned char *pixel_ptr;
    register int width = given_width;
    int byte_order = ImageByteOrder( dpy );
    
    r = rgb[0];

    pixel_ptr = (unsigned char *) ( image->data + y * image->bytes_per_line );
    
    while (--width >= 0) {
	pixval = img->pixel_table[ *r ];
	if ( byte_order == MSBFirst )
	{
	    pixel_ptr += 3;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr-- = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr = pixval & 0xff;
	    pixel_ptr += 4;
	}
	else
	{
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	    pixval >>= 8;
	    *pixel_ptr++ = pixval & 0xff;
	}
	r += stride;
    }
    
}
static
void
MAG_1_mono_color_32 ( img, rle_x, rle_y, mag_size, x, y, width, height, image )
image_information *img;
int		rle_x, rle_y;
int		width, height;
int		mag_size;
int		x, y;
XImage		*image;

{
    register unsigned char *r;
    register unsigned char *pixel_ptr;
    register unsigned long pixval;
    unsigned long pixel_value;
    register int 	mag_x;
    register int 	w;
    register int 	mag_y = mag_size;
    unsigned char 	*line_ptr, *last_line;
    unsigned char	*rgb_line, *last_rgb;
    int			rgb_line_stride = img->w * img->dpy_channels;
    int byte_order = ImageByteOrder( dpy );

    rgb_line = last_rgb = SAVED_RLE_ROW( img, rle_y ) + rle_x;
    
    last_line = line_ptr = (unsigned char *)
	(image->data + (y * image->bytes_per_line)) + 4 * x;

    while (--height >= 0) {
	if ( rgb_line == last_rgb && last_line != line_ptr )
	    memcpy( line_ptr, last_line, width * sizeof( long ) );
	else
	{
	    pixel_ptr = line_ptr;
	    w = width;
	    r = rgb_line;
	    mag_x = mag_size;
	    
	    pixel_value = img->pixel_table[ *r ];
    
	    while (--w >= 0) {
		pixval = pixel_value;
		if ( byte_order == MSBFirst )
		{
		    pixel_ptr += 3;
		    *pixel_ptr-- = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr-- = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr-- = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr = pixval & 0xff;
		    pixel_ptr += 4;
		}
		else
		{
		    *pixel_ptr++ = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr++ = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr++ = pixval & 0xff;
		    pixval >>= 8;
		    *pixel_ptr++ = pixval & 0xff;
		}
		if ( --mag_x == 0 )
		{
		    r++; 
		    mag_x = mag_size;
		    
		    pixel_value = img->pixel_table[ *r ];
		}
	    }
	}
	last_line = line_ptr;
	last_rgb = rgb_line;

	line_ptr += image->bytes_per_line;
	if ( --mag_y == 0 )
	{
	    rgb_line += rgb_line_stride;
	    mag_y = mag_size;
	}
    }
}

/*****************************************************************
 * TAG( map_rgb_to_bw )
 * 
 * Convert RGB to black and white through NTSC transform, but map
 * RGB through a color map first.
 * Inputs:
 * 	red_row, green_row, blue_row:	Given RGB pixel data.
 *	map:		Array[3] of pointers to pixel arrays,
 *			representing color map.
 *	rowlen:		Number of pixels in the rows.
 * Outputs:
 * 	bw_row:	    Output B&W data.  May coincide with one of the
 *		    inputs.
 * Algorithm:
 * 	BW = .35*map[0][R] + .55*map[1][G] + .10*map[2][B]
 */
void
map_rgb_to_bw( img, rows, bw_row )
image_information *img;
rle_pixel 	      **rows;
register rle_pixel     *bw_row;

{
    register rle_pixel *red;
    register rle_pixel *green;
    register rle_pixel *blue;

    rle_pixel	**map;
    int		ncolors;
    int		rowlen;

    map = img->in_cmap;
    ncolors = img->img_channels;
    rowlen = img->w;
    
    if (ncolors < 1) {
	fprintf (stderr, "%s: map_rgb_to_bw given %d colors\n", 
		 progname, ncolors);
	exit (1);
    }
    
    switch (ncolors) {
	
    case 1:
	red = rows[0];
	if ( !map || img->mono_color )
	    duff8( rowlen, *bw_row++ = *red++)
	else {
	    register rle_pixel *cmap = map[0];
	    duff8( rowlen, *bw_row++ = cmap[*red++])
	}
	break;
	
    case 2:
	red = rows[0];
	green = rows[1];
	if ( !map )
	    duff8( rowlen, *bw_row++ = ( 50 * *red++ + 50 * *green++ ) / 100)
	else {
	    register rle_pixel **cmap = map;
	    duff8( rowlen, *bw_row++ = (50 * cmap[0][*red++] +
					50 * cmap[1][*green++]) /100)
	}
	break;
	
    default:
    case 3:
	red = rows[0];
	green = rows[1];
	blue = rows[2];
	if ( !map )
	    duff8( rowlen, *bw_row++ = ( 35 * *red++ + 55 * *green++ +
					10 * *blue++ ) / 100)
	else {
	    register rle_pixel **cmap = map;
	    duff8( rowlen, *bw_row++ = ( 35 * cmap[0][*red++] +
					55 * cmap[1][*green++] +
					10 * cmap[2][*blue++] ) / 100)
	}
	break;
    }
}

/*****************************************************************
 * TAG( map_rgb_to_rgb )
 * 
 * Convert RGB to RGB through a colormap
 * Inputs:
 * 	in_rows:	Given RGB pixel data.
 *	map:		Array[3] of pointers to pixel arrays,
 *			representing color map.
 *	rowlen:		Number of pixels in the rows.
 * Outputs:
 * 	out_rows:       Output data.  May coincide with one of the inputs.
 */
void
map_rgb_to_rgb( img, in_rows, out_rows )
image_information *img;
rle_pixel 	**in_rows, **out_rows;
{
    register rle_pixel *in, *out, *cmap;
    register int w;
    rle_pixel **map;
    int	ncolors;
    int	rowlen;
    
    map = img->in_cmap;
    ncolors = img->img_channels;
    rowlen = img->w;

    if ( ncolors < 1 )
    {
	fprintf (stderr, "%s: map_rgb_to_rgb given %d colors\n", 
		 progname, ncolors);
	exit (1);
    }	
    
    if ( map )
	while ( --ncolors >= 0 )
	{
	    in = in_rows[0];
	    out = out_rows[0];
	    cmap = map[0];
	    w = rowlen;
	    
	    duff8( w, *out++ = cmap[*in++] );
	    
	    in_rows++;
	    out_rows++;
	    map++;
	}
    else 
	while ( --ncolors >= 0 )
	{
	    if ( in_rows[0] != out_rows[0] )
		memcpy( out_rows[0], in_rows[0], rowlen );

	    in_rows++;
	    out_rows++;
	}
}


void get_dither_arrays( img )
register image_information *img;
{
    if (!img->divN)
	img->divN = (int *) malloc ( 256 * sizeof(int) );
    if (!img->modN)
	img->modN = (int *) malloc ( 256 * sizeof(int) );
    if (!img->dm16)
	img->dm16 = (array16 *) malloc ( 16 * 16 * sizeof(int) );
    
    if (!img->divN || !img->modN || !img->dm16 )
    {
	fprintf( stderr, "%s: malloc error getting dither arrays\n");
	exit (1);
    }
}


int
shift_match_left (mask, high_bit_index)

Pixel	mask;
int	high_bit_index;

{
    register int	shift;
    register Pixel	high_bit;
    
    if (mask == 0) return (0);
    
    high_bit = 0x80000000;
    
    for (shift = (32 - high_bit_index); (high_bit & mask) == 0; shift--) {
	high_bit >>= 1;
    }
    
    return (shift);
    
}



int
shift_match_right (mask)
Pixel	mask;
{
    register int	shift;
    register Pixel	low_bit;
    
    if (mask == 0) return (0);
    
    low_bit = 1;
    
    for (shift = 0; (low_bit & mask) == 0; shift++) {
	low_bit <<= 1;
    }
    
    return (shift);
    
}
