/* 
 * getbob.c - Read RLE files onto hp bobcat screens.
 * 
 * Author:	Mark Bloomenthal
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Fri Oct 17 1986
 * Copyright (c) 1986, University of Utah
 *
 * Most code lifted from J W. Peterson.
 *
 * Flags are:
 *   -l		Linear map
 *   -g gam	Gammap map, use gamma of gam (floating point number)
 *   -d device	Use the device specified.
 *   -x driver	Use the driver specified.
 *   -p x y	Position image lower left hand corner on the display.  The
 *		x and y position is given in pixels with the origin taken as
 *		the lower left hand corner of the display.  This flag is only
 *		useful with the -D, -d, and/or -x flags.
 *
 */

#include "starbase.c.h"
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include "rle.h"

typedef char * string;                  /* Character Strings. */
typedef int boolean;                    /* Logical vars or values. */
#define TRUE  1                         /* Logical constants. */
#define FALSE 0
typedef float single;
#define MAX(i,j)   ( (i) > (j) ? (i) : (j) )


/* Some starbase type definitions. */
typedef single starbase_color_type[3];
typedef unsigned char starbase_color_index_type;

#define RASTERSIZE_LIM 1023

/* Standard color map offset. */
#define COLMAP_OFFSET	    8

/* Color modes. */
#define MONOCHROME_MODE		1
#define EIGHT_BIT_COLOR_MODE    2

/* Scanline storage & RLE row pointers */
unsigned char scanline[4][RASTERSIZE_LIM], *rows[4];

/* Scanline to be written to device. */
starbase_color_index_type dest_pixels[RASTERSIZE_LIM];  /* avoid bludging stack */

/* Dither matrix. */
int dm16[16][16];

/* Tables for quantization. */
int errN[256], divN[256];

/* The gamma map and default gamma. */
int gammamap[256];
float gam = 2.0;

/* Input filename. */
string infname = NULL;

/* Image header. */
rle_hdr hdr;

/* Color map flags. */
boolean linear_flag = FALSE;
boolean gamma_flag = FALSE;

boolean dummy;

/* The default device and device driver. */
string  display_name = "/dev/crt";
string  driver_name = "hp98705";

/* For positioning the image on the display. */
boolean pos_flag;
int pic_x = 0;
int pic_y = 0;

/* How many bits in the devices color map. */
int dev_color_map_size;

/* What mode to dither into.  Either 8 bit color or monochrome. */
int put_mode;

/* Color map to be written to device. */
int n_colmap_colors;
static starbase_color_type colmap[216];

/* The color to be used in case we do monochrome. */
float r_base_color = 1.0;
float g_base_color = 1.0;
float b_base_color = 1.0;

/* Starbase file descriptor for device where the picture goes. */
int picture_fd;


main( argc, argv)
int argc;  char *argv[];
{
    int i;
    int row;
     boolean bw_flag = FALSE;

    /* sbrk( 1000000 ); */

    /* Get command line arguments. */
    if ( scanargs( argc, argv,
    "getbob l%- g%-gamma!f p%-pos!d!d d%-display!s x%-driver!s file%s",
		   &linear_flag, &gamma_flag, &gam,
		   &pos_flag, &pic_x, &pic_y,
		   &dummy, &display_name,
		   &dummy, &driver_name,
		   &infname )  == 0 )
	exit( 1 );

    /* Open file for reading, if no filename was specified, we'll read
     * from stdin.
     */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), infname, 0 );
    hdr.rle_file = rle_open_f(hdr.cmd, infname, "r");
    /* Read header information from rle file. */
    rle_get_setup_ok( &hdr, NULL, NULL );

    RLE_CLR_BIT( hdr, RLE_ALPHA );	/* No alpha channel */

    /* Set up gamma correction tables if need be. */
    init_gamma_map();

    direct_setup();

    /* Set up rows to point to our copy of the scanline */
    for ( i= 0; i < 4; i++ )
	rows[i] = scanline[i];

    /* Get the scanlines. */
    for (i = hdr.ymin; i < hdr.ymax; i++)
    {
	rle_getrow( &hdr, rows );
	if ( put_mode == MONOCHROME_MODE )
	    put_line_mono( i );
	else
	    put_line_8( i );
    }
}



/*****************************************************************
 * TAG( init_gamma_map )
 * 
 * Compute a gamma correction map.
 */

init_gamma_map()
{
    int i;

    /* Compute a gamma correction map. */
    for ( i = 0; i < 256; i++ )
    {
	if ( ! linear_flag )
	{
	    gammamap[i] = (int)(0.5 + 255 * pow( i / 255.0, 1.0/gam ));
	}
    }
}



/*****************************************************************
 * TAG( init_8_bit_color_map )
 * 
 * Initialize the 8 bit color map.
 */

init_8_bit_color_map()
{
    int i;

    /* 
     * Set up the color map entries.  We will use 216 colors.
     */
    n_colmap_colors = 216;		/* Fill out the global. */

    for ( i = 0; i < n_colmap_colors; i++ )
    {
	colmap[i][0] = ((i%6) * 51) / 255.0;
	colmap[i][1] = (((i/6)%6) * 51) / 255.0;
	colmap[i][2] = (((i/36)%6) * 51) / 255.0;
    }

    make_square( 6.0, divN, errN, dm16 );
}



/*****************************************************************
 * TAG( init_monochrome_color_map )
 * 
 * Initialize the monochrome color map.  This is for devices with
 * color maps too small to do good color dithering.  The argument
 * ncolors is the number of colors of the devices color map that
 * may be used for dithering.
 */

init_monochrome_color_map( ncolors )
int ncolors;
{
    int k;
    int color_val;

    /* Set up the color map entries into a single grayscale ramp. */
    n_colmap_colors = ncolors;		/* Fill out the global. */

    for ( k = 0; k < ncolors; k++ )
   {
	color_val = (k * 255) / (ncolors - 1);
	colmap[k][0] = (color_val * r_base_color) / 255.;
	colmap[k][1] = (color_val * g_base_color) / 255.;
	colmap[k][2] = (color_val * b_base_color) / 255.;
    }

    /* Make dithering tables. */
    make_square( (double)ncolors, divN, errN, dm16 );
}



#define DMAP(v,x,y)	(errN[v]>dm16[x][y] ? divN[v] + 1 : divN[v])

/*****************************************************************
 * TAG( put_line_8 )
 * 
 * Map a 24 bit scanline to 8 bits through the dither matrix.
 */

put_line_8( y )
int y;
{
    register unsigned char *r, *g, *b;
    register int i, dither_col, dither_row;
    register starbase_color_index_type *dest_pixel_ptr;
    int xmax = hdr.xmax;
    int g_offset, b_offset;
    

    /* In case we have less than 3 color channels. */
    for (i = 2; i >= hdr.ncolors; i--)
	bcopy(rows[0], rows[i], hdr.xmax - hdr.xmin);

    dither_row = y % 16;
    dither_col = 0;
    r = rows[0];
    g = rows[1];
    b = rows[2];
    dest_pixel_ptr = &dest_pixels[0];

    /* Linear map. */
    if ( linear_flag )
	for ( i = 0; i < xmax; i++, r++, g++, b++,
	      		       dither_col = ((dither_col + 1) & 15),
	      		       dest_pixel_ptr++ )
	    *dest_pixel_ptr =
		DMAP( *r, dither_col, dither_row ) +
		DMAP( *g, dither_col, dither_row ) * 6 +
		DMAP( *b, dither_col, dither_row ) * 36 +
		COLMAP_OFFSET;

    /* RLE file with color map. */
    else if ( hdr.ncmap )
    {
	/* Compute offsets to the green and blue sections of the color map. */
	g_offset = 1 << hdr.cmaplen;
	b_offset = 2 * g_offset;

	for ( i = 0; i < xmax; i++, r++, g++, b++,
	      		       dither_col = ((dither_col + 1) & 15),
	      		       dest_pixel_ptr++ )
	    *dest_pixel_ptr =
		DMAP( hdr.cmap[*r], dither_col, dither_row ) +
		DMAP( hdr.cmap[*g + g_offset],
					dither_col, dither_row ) * 6 +
		DMAP( hdr.cmap[*b + b_offset],
					dither_col, dither_row ) * 36 +
		COLMAP_OFFSET;
    }

    /* Gamma correction is the default. */
    else
	for ( i = 0; i < xmax; i++, r++, g++, b++,
	      		       dither_col = ((dither_col + 1) & 15),
	      		       dest_pixel_ptr++ )
	    *dest_pixel_ptr =
		DMAP( gammamap[*r], dither_col, dither_row ) +
		DMAP( gammamap[*g], dither_col, dither_row ) * 6 +
		DMAP( gammamap[*b], dither_col, dither_row ) * 36 +
		COLMAP_OFFSET;

    /* Write this scanline to the device. */
    write_scanline( y );
}



/*****************************************************************
 * TAG( put_line_mono )
 * 
 * For small color maps.  Dither into monochrome.
 */

put_line_mono( y )
int y;
{
    register unsigned char *r, *g, *b;
    register int i, dither_col, dither_row;
    register starbase_color_index_type *dest_pixel_ptr;
    int xmax = hdr.xmax;
    int bw_val;
    int g_offset, b_offset;

    /* In case we have less than 3 color channels. */
    for (i = 2; i >= hdr.ncolors; i--)
	bcopy(rows[0], rows[i], hdr.xmax - hdr.xmin);

    dither_row = y % 16;
    dither_col = 0;
    r = rows[0];
    g = rows[1];
    b = rows[2];
    dest_pixel_ptr = &dest_pixels[0];

    /* Linear map. */
    if ( linear_flag )
	for ( i = 0; i < xmax; i++, r++, g++, b++,
	      		       dither_col = ((dither_col + 1) & 15),
	      		       dest_pixel_ptr++ )
	{
	    bw_val =
		(35*(*r) + 55*(*g) + 10*(*b)) / 100;
	    *dest_pixel_ptr =
		DMAP( bw_val, dither_col, dither_row ) + COLMAP_OFFSET;
	}

    /* RLE file with color map. */
    else if ( hdr.ncmap )
    {
	/* Compute offsets to the green and blue sections of the color map. */
	g_offset = 1 << hdr.cmaplen;
	b_offset = 2 * g_offset;

	for ( i = 0; i < xmax; i++, r++, g++, b++,
	      		       dither_col = ((dither_col + 1) & 15),
	      		       dest_pixel_ptr++ )
	{
	    bw_val =
		( 35 * hdr.cmap[*r] +
		  55 * hdr.cmap[*g + g_offset] +
		  10 * hdr.cmap[*b + b_offset] ) / 100;
	    *dest_pixel_ptr =
		DMAP( bw_val, dither_col, dither_row ) + COLMAP_OFFSET;
	}
    }

    /* Gamma correction is the default. */
    else
	for ( i = 0; i < xmax; i++, r++, g++, b++,
	      		       dither_col = ((dither_col + 1) & 15),
	      		       dest_pixel_ptr++ )
	{
	    bw_val =
		(35*gammamap[*r] + 55*gammamap[*g] + 10*gammamap[*b]) / 100;
	    *dest_pixel_ptr =
		DMAP( bw_val, dither_col, dither_row ) + COLMAP_OFFSET;
	}

    /* Write this scanline to the device. */
    write_scanline( y );
}



/*****************************************************************
 * TAG( direct_setup )
 * 
 * Use starbase to talk directly to graphics device.
 */

direct_setup()
{
    single p_lim[2][3], res[3], p1[3], p2[3];
    int map_size;
    int dev_xmax, dev_ymax;

    picture_fd = gopen( display_name, OUTDEV, driver_name, 0 );
    if ( picture_fd < 0 )
    {
	fprintf( stderr, "Can't open device.\n" );
	exit( 1 );
    }

    /* Find out info about this device. */
    inquire_sizes( picture_fd, p_lim, res, p1, p2, &map_size );

    if ( map_size >= 256 )
    {
	put_mode = EIGHT_BIT_COLOR_MODE;
	init_8_bit_color_map();
    }
    else
    {
	put_mode = MONOCHROME_MODE;
	init_monochrome_color_map( map_size - COLMAP_OFFSET );
    }

    write_color_map( picture_fd );

    /* Set the screen limits in pixels so we can use starbase in a device
     * independent way.
     *
     * It is assumed that the p_lim values returned by inquire_sizes are the
     * lower left screen coordinates in (float) pixels and the upper right
     * screen coordinates in (float) pixels.  It is also assumed that the
     * range of values in x and y coordinates are given from 0 to some
     * positive maximum value.
     */
    dev_xmax = MAX( p_lim[0][0], p_lim[1][0] );
    dev_ymax = MAX( p_lim[0][1], p_lim[1][1] );

    if ( hdr.xmax > dev_xmax )
	hdr.xmax = dev_xmax;
    if ( hdr.ymax > dev_ymax )
	hdr.ymax = dev_ymax;

    /* Make sure that the specified lower left hand corner of the image
     * will keep the image in bounds.
     */
    if ( ( hdr.xmax + pic_x ) > dev_xmax )
	pic_x = dev_xmax - hdr.xmax;
    if ( ( hdr.ymax + pic_y ) > dev_ymax )
	pic_y = dev_ymax - hdr.ymax;

    /* Set to use the whole display surface. */
    mapping_mode( picture_fd, DISTORT );

    /* Set to address the pixels in a device independent way. */
    vdc_extent( picture_fd, 0.0, 0.0, 0.0,
		(single)dev_xmax, (single)dev_ymax, 0.0 );
    clip_rectangle( picture_fd, 0.0, (single)dev_xmax, 0.0, (single)dev_ymax );

}



/*****************************************************************
 * TAG( write_color_map )
 * 
 * Write color map to device.
 *
 */

write_color_map( star_fd )
int star_fd;
{
    define_color_table( star_fd, COLMAP_OFFSET, n_colmap_colors, colmap );

}



/*****************************************************************
 * TAG( write_scanline )
 * 
 * Write scanline to device.
 *
 */

write_scanline( y )
int y;
{
    block_write( picture_fd,
		   (single)(hdr.xmin + pic_x), (single)(y + pic_y),
		   hdr.xmax - hdr.xmin, 1,
		   dest_pixels, FALSE );
}


#ifdef	NEED_BSTRING

/*****************************************************************
 * TAG( bcopy )
 *
 * Move contents of a block to a new location.
 * For systems without a bcopy system routine.
 */
void bcopy( src_ptr, dst_ptr, size )
register int * src_ptr, * dst_ptr;
int size;
{
    register int words;
    register char * byte_src_ptr, * byte_dst_ptr;
    register int bytes;

    /* Tight word copy loop for most or all of the block. */
    words=size/sizeof(int);
    bytes = size - words*sizeof(int);
    while ( words-- ) 
	*dst_ptr++ = *src_ptr++;

    /* Tight byte copy loop for the remainder. */
    byte_src_ptr = (char *)src_ptr;
    byte_dst_ptr = (char *)dst_ptr;
    while ( bytes-- ) 
	*byte_dst_ptr++ = *byte_src_ptr++;
}
#endif
