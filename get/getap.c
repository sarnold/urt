/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
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
 * getap.c - Read RLE files onto apollo screens.
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Fri Jun 27 1986
 * Copyright (c) 1986 John W. Peterson
 *
 * Revision 1.1  87/05/26  13:22:30  Clark
 * Add window name overhead to create window call and fix y calculation in
 * put_line.
 * Revision 1.2  87/10/01  10:38:00  Clark
 * Add code to wait for user input, quit when user types "q"
 * and to screen dump a GMR bitmap file if user types "d"
 * Revision 1.3  88/01/07  08:28:30  Clark
 * Add code to put file name in window legend and fix for obscured
 * window, add flag for no border and x,y position
 * Revision 1.4  88/01/08  08:10:30  Clark
 * Add code for adding text to image
 * Revision 1.5  88/02/25  01:54:00  Clark
 * Re-instate BW code, and add suport for 4 bit plane workstations.
 * Revision 1.6  88/02/29  11:33:50  Clark
 * Use all available bit planes for gray scale. Use 16x16 dither
 * for BW nodes instead of 8x8 S. Kitaoka Grey table
 *
 *
 * Some code lifted from an old getX by Spencer Thomas and showbw
 * by Jim Schimpf.
 *
 * Flags are:
 *   -l		Linear map
 *   -g gam	Gammap map, use gamma of gam (floating point number)
 *   -b		Use full 24 bit mode (DN550, DN560, DN660 only)
 *   -w         Black and white
 *   -r		Flip white on black/black on white for grayscale display.
 *   -x left	X coordinate of window
 *   -y top	Y coordinate of window
 *   -t text	Add text to bottom of image
 *   -n 	No border or legend
 *
 * NOTE: This version applies the color map if present.
 */

#include <stdio.h>
#include <math.h>
#include "rle.h"

#include "/sys/ins/base.ins.c"
#include "/sys/ins/gpr.ins.c"
#include "/sys/ins/error.ins.c"
#include "/sys/ins/pad.ins.c"

#define RASTERSIZE 1023
#define COLMAP_OFFSET 16

#define sysdebug if ((status.all)!=0) error_$print(status)

/* Scanline storage & RLE row pointers */
rle_pixel **rows;
#define DMAP(v,x,y)	(modN[v]>dm16[x][y] ? divN[v] + 1 : divN[v])
int dm16[16][16];
int modN[256], divN[256];
float gam = 2.0;		/* Default gamma */
int inverse_flag = false;	/* If true, swap bits on B&W displays */
int gray_flag = false;		/* True if making BW image on color screen */
int borrow_mode = false;	/* DN560/660 imaging mode (borrow screen)) */
int 
    linear_flag = false,	/* Color map flags */
    gam_flag = true;
int four_flag = false;		/* If true, workstation has only 4 planes */
int bw_flag = false;		/* If true, workstation is BW */
int x_flag = false;		/* True if X coord specified */ 
int y_flag = false;		/* True if Y coord specified */ 
int no_border_flag = false;     /* True if no border or legend */
int left = 0;			/* Default window X coord */
int top = 0;			/* Default window Y coord */
gpr_$pixel_value_t dest_pixels[RASTERSIZE];  /* avoid bludging stack */
gpr_$offset_t size;
short font_id;
short middle;
status_$t status;
gpr_$keyset_t keyset = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
                         0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
char key;
gpr_$position_t position;
boolean obs;
gpr_$event_t event;
gpr_$version_t version;
gpr_$offset_t dump_size;
gpr_$window_t dump_window;
gpr_$attribute_desc_t attribs;
gpr_$bitmap_desc_t bitmap;
gpr_$bitmap_desc_t dump_bitmap;
boolean created;
gpr_$bmf_group_header_array_t header;
gpr_$color_vector_t color_map;
short map[3][256];

/* Image header. */
rle_hdr	hdr;

main(argc, argv)
int argc;
char **argv;
{
    int i;
    short len;
    int row;
    unsigned n;
    char * fname = NULL;
    char * text = NULL;
    int text_flag = 0;
    short namelen, tok_count;
    char gstring[20];

    set_sbrk_size( 1000000 );

    hdr = *rle_hdr_init( (rle_hdr *)NULL );

    if (! scanargs( argc, argv,
		   "% b%- l%- r%- w%- n%- g%-gamma!f x%-left!d y%-top!d t%-text!s file%s",
		   &borrow_mode, &linear_flag, &inverse_flag,
		   &gray_flag, &no_border_flag, &gam_flag, &gam,
		   &x_flag, &left, &y_flag, &top, &text_flag, &text, &fname ))          
	exit( 1 );
    rle_names( &hdr, cmd_name( argv ), fname, 0 );
    hdr.rle_file = rle_open_f(hdr.cmd, fname, "r");

    switch( (int) rle_get_setup( &hdr ) )
    {
    case 0:
	break;			/* successful open */
    case -1:			/* not an RLE file */
    case -4:			/* or header is short */
	fprintf( stderr, "getap: %s is not an RLE file\n", fname );
	exit(-4);
	break;
    case -2:
	fprintf( stderr, "getap: malloc failed\n" );
	exit(-2);
	break;
    case -3:
	fprintf( stderr, "getap: input file is empty\n" );
	exit(-3);
	break;
    }

/*    RLE_CLR_BIT( hdr, RLE_ALPHA );	/* No alpha channel */

    rle_row_alloc( &hdr, &rows );

    if (borrow_mode)
    {
	borrow_setup();
	borrow_color_map(); 
	if (hdr.ymax > 511)
	    hdr.ymax = 511;
    }
    else
    {
	setup_scr(hdr.xmax, hdr.ymax);
	window_color_map();
	if (hdr.ymax > RASTERSIZE)
	    hdr.ymax = RASTERSIZE;
    }

    if (bw_flag) gray_flag = true;

    for (i = hdr.ymin; i <= hdr.ymax; i++)
    {
	rle_getrow( &hdr, rows );
	if (borrow_mode)
	    put_line_borrow(i);
	else
	{
	    if (gray_flag)
		put_linebw(i);
	    else
	    {
		if (four_flag)
		    put_line4(i);
		else
		    put_line8(i);
	    }
	}
    }
    if (text_flag)
    {
	if (!borrow_mode) gpr_$acquire_display( status ) ;
	gpr_$load_font_file( "nonie.r.16", (short) 10, font_id, status );
	gpr_$set_text_font( font_id, status );
	gpr_$set_text_background_value( (int) -1, status );
	gpr_$set_text_value( (int) 17, status );
	gpr_$inq_text_extent( *text, (short) strlen(text),
			      size, status );
	middle = (hdr.xmax/2) - (size.x_size/2);
	if (middle < 0) middle=0;
	gpr_$move( middle, (short)(hdr.ymax-15), status);
	gpr_$text( *text, (short) strlen(text), status);
	if (!borrow_mode) gpr_$release_display( status ) ;
    }
    do
    {
	if (!borrow_mode) gpr_$acquire_display( status ) ;
	gpr_$enable_input( gpr_$keystroke, keyset, status ) ;
	obs = gpr_$event_wait ( event, key, position, status ) ;
	if (!borrow_mode) gpr_$release_display( status ) ;
	if ( (event == gpr_$keystroke) && ( (key == 'd') || (key == 'D') ) )
	{
	    if (borrow_mode)
	    {
		gpr_$inq_bitmap( bitmap, status ) ;
		sysdebug;
		gpr_$allocate_attribute_block(attribs,status);
		sysdebug;
		header[0].n_sects	= 8;
		header[0].pixel_size     = 1;
		header[0].allocated_size = 0;
		header[0].bytes_per_line = 0;
		header[0].bytes_per_sect = 0;
		dump_size.x_size = 1024;
		dump_size.y_size = 1024;
		gpr_$open_bitmap_file(gpr_$create,"screen_dump",(short)11,version,
		    dump_size,(short)1,header,attribs,dump_bitmap,created,status);
		if (status.all == 0)
		{
		    gpr_$set_bitmap(dump_bitmap,status);
		    sysdebug;
		    gpr_$inq_color_map((int)0,256,color_map,status);
		    sysdebug;
		    gpr_$set_bitmap_file_color_map(dump_bitmap,0,256,color_map,status);
		    sysdebug;
		    dump_window.window_base.x_coord = 0;
		    dump_window.window_base.y_coord = 0;
		    dump_window.window_size.x_size = 1024;
		    dump_window.window_size.y_size = 1024;
		    gpr_$set_imaging_format(gpr_$interactive, status );
		    sysdebug;
		    gpr_$pixel_blt(bitmap,dump_window,dump_window.window_base,status);
		    sysdebug;
		    gpr_$set_imaging_format(gpr_$imaging_512x512x24, status );
		    sysdebug;
		}
		else
		{
		    fprintf(stderr, "getap: Could not open bitmap file screen_dump - ");
		    error_$print(status);
		}
	    }
	    else
	    {   
		pad_$dm_cmd ((short)1, "CPO /COM/CPSCR SCREEN_DUMP -GPR", (short)31, status ) ;
		sysdebug;
	    }
	}
	if ( (event == gpr_$keystroke) && ( (key == 'p') || (key == 'P') && (!borrow_mode) ) )
	{
	    pad_$dm_cmd ((short)1, "WP", (short)2, status ) ;
	    sysdebug;
	}
    }
    while ( !( (event == gpr_$keystroke) && ( (key == 'q') || (key == 'Q') ) ) );
    if (borrow_mode)
    {
	gpr_$set_imaging_format(gpr_$interactive, status );
	sysdebug;
    }
    if (!borrow_mode && four_flag)
    {
	gpr_$acquire_display( status ) ;
	gpr_$set_color_map((int)0,8,color_map,status);
	gpr_$release_display( status ) ;
    }
}

/*
 * Initialize the GPR package to imaging mode
 */
borrow_setup()
{
    gpr_$offset_t size;
    gpr_$bitmap_desc_t main_bitmap;      /* Master bitmap */

    size.x_size = 1024;
    size.y_size = 1024;			/* GPR will shrink to fit */

    gpr_$init(gpr_$borrow, 1, size, 7, main_bitmap, status );
    sysdebug;

    gpr_$set_imaging_format( gpr_$imaging_512x512x24, status );
    sysdebug;
    if (status.all != status_$ok)
    {
	fprintf(stderr, "getap: Wrong hardware?\n");
	exit(-1);
    }
}

/* Set up the color map (imaging mode) */
borrow_color_map()
{
    unsigned int value;
    unsigned int intgam;
    int i, map_offset;
    gpr_$color_vector_t colmap;
    
    if ( hdr.ncmap )	/* Must read color map */
    {
	map_offset = 1 << hdr.cmaplen;

	/* This needs more thought; handle ncmap = 1, etc. */
	if (hdr.ncmap == 3)

	for (i=0; i < 256; i++)
	{
            /* This is bogus, we need to index cmap!... */
	    
	    value = (((hdr.cmap[i]>> 8) & 0xFF) << 16);
	    value |= (((hdr.cmap[i+map_offset] >> 8) & 0xFF) << 8);
	    value |= ((hdr.cmap[i+2*map_offset] >> 8) & 0xFF);
	    colmap[i] = value;
	}
    }
    else
    {	  
	if (gam_flag)
	{
	    for (i = 0; i < 256; i++)
	    {
		intgam = (int)(0.5 + 255 * pow( i / 255.0, 1.0/gam ));
		value = intgam;
		value |= (intgam << 8);
		value |= (intgam << 16);
		colmap[i] = value;
	    }
	}
	else
	{			/* Default is linear map */
	    for (i=0; i < 256; i++)
	    {
		value = (i << 16);
		value |= (i << 8); 
		value |= i;
		colmap[i] = value;
	    }
	}
    }
	    
	  
    gpr_$set_color_map( (int) 0, 256, colmap, status );
    sysdebug;
}

/*
 * Initialize the 8 bit color map.  Use gamma corrected map.
 */
window_color_map()
{
    int i, j, k, l, planes, map_offset;
    unsigned int value;
    unsigned int intgam;
    status_$t status;
    static gpr_$color_vector_t colmap;
    gpr_$display_config_t config;

    if ( hdr.ncmap )	/* Must read color map */
    {
	map_offset = 1 << hdr.cmaplen;

	/* This needs more thought; handle ncmap = 1, etc. */
	if (hdr.ncmap == 3)

	for (i=0; i < 256; i++)
	{
            /* This is bogus, we need to index cmap!... */
	    
	    map[0][i] = ((hdr.cmap[i]>> 8) & 0xFF);
	    map[1][i] = ((hdr.cmap[i+map_offset] >> 8) & 0xFF);
	    map[2][i] = ((hdr.cmap[i+2*map_offset] >> 8) & 0xFF);
	}
    }
    else
    {	  
	if (gam_flag)
	{
	    for (i = 0; i < 256; i++)
	    {
		intgam = (int)(0.5 + 255 * pow( i / 255.0, 1.0/gam ));
		map[0][i] = intgam;
		map[1][i] = intgam;
		map[2][i] = intgam;
	    }
	}
	else
	{			/* Default is linear map */
	    for (i=0; i < 256; i++)
	    {
		map[0][i] = i;
		map[1][i] = i;
		map[2][i] = i;
	    }
	}
    }

    gpr_$inq_config( config, status );
    if ((config == gpr_$color_1024x1024x4) ||
	(config == gpr_$color_1024x800x4) ||
	(config == gpr_$color2_1024x800x4)) four_flag = true;
    if ((config == gpr_$bw_800x1024) ||
	(config == gpr_$bw_1024x800) ||
	(config == gpr_$bw_1280x1024)) bw_flag = true;

    if (four_flag && !bw_flag && !gray_flag)    
    {
	/* 
	 * Set up the 4-bit color map entries.
	 */
	for(i = 0; i < 8; i++) {
	    colmap[i] = ((i%2) * 255) << 16;
	    colmap[i] |= (((i/2)%2) * 255) << 8;
	    colmap[i] |= (((i/4)%2) * 255);
	}

	/* Save DM window colors */
	gpr_$acquire_display( status );
	gpr_$inq_color_map((int)0,8,color_map,status);
	gpr_$set_color_map( (int)0, 8, colmap, status );
	gpr_$release_display( status );

	make_square( 2.0, divN, modN, dm16 );
    }
    else if (four_flag && !bw_flag && gray_flag)
    {
        bwdithermap( 8, gam, colmap, divN, modN, dm16);

	for(i = 0; i < 8; i++) {
            j = colmap[i];
	    colmap[i] = j << 16;
	    colmap[i] |= j << 8;
	    colmap[i] |= j;
	}
	/* Save DM window colors */
	gpr_$acquire_display( status );
	gpr_$inq_color_map((int)0,8,color_map,status);
	gpr_$set_color_map( (int)0, 8, colmap, status );
	gpr_$release_display( status );

    }
    else if (!bw_flag && !gray_flag)
    {
	/* 
	 * Set up the 8 bit color map entries.  We don't yet know the location
	 * in the map at which each will reside, so init it to 0.
	 */
	for(i = 0; i < 216; i++) {
	    colmap[i] = ((i%6) * 51) << 16;
	    colmap[i] |= (((i/6)%6) * 51) << 8;
	    colmap[i] |= (((i/36)%6) * 51);
	}

	/* Offset from DM window colors */
	gpr_$acquire_display( status );
	gpr_$set_color_map( (int) COLMAP_OFFSET, 256 - COLMAP_OFFSET,
        	           colmap, status );
	gpr_$release_display( status );

	/* Compute tables */
	make_square( 6.0, divN, modN, dm16 );
    }
    else if (!bw_flag)
    {
        bwdithermap( 240, gam, colmap, divN, modN, dm16);

	for(i = 0; i < 240; i++) {
            j = colmap[i];
	    colmap[i] = j << 16;
	    colmap[i] |= j << 8;
	    colmap[i] |= j;
	}
	/* Offset from DM window colors */
	gpr_$acquire_display( status );
	gpr_$set_color_map( (int) COLMAP_OFFSET, 256 - COLMAP_OFFSET,
        	           colmap, status );
	gpr_$release_display( status );
    }
    else
    {
        bwdithermap( 2, gam, colmap, divN, modN, dm16);
    }
}

/* All the fun stuff to set up an apollo window */
setup_scr(hsize,vsize)
int hsize,vsize;
{
    gpr_$offset_t size;
    gpr_$color_vector_t colmap;
    gpr_$attribute_desc_t hidden_desc;
    unsigned int value;
    int i;
    name_$pname_t output_file;
    short len, w, h;
    pad_$window_desc_t window_shape;
    static gpr_$bitmap_desc_t main_bitmap;
    short fontid;
    static stream_$id_t out_stream;

    /* Open a separate window to display the results */

    window_shape.top = 0;
    window_shape.left = 0;
    if ( !no_border_flag )
    {
	w = 11;
	h = 34;
    }
    else
    {
	w = 1;
	h = 1;
    }
    window_shape.width = (short)hsize + w;
    window_shape.height = (short)vsize + h;

    pad_$create_window( "", (short) 0, pad_$transcript, (short) 1,
			window_shape, out_stream, status );

    if ( no_border_flag ) pad_$set_border( out_stream, (short) 1, false, status );

    window_shape.top = top;
    window_shape.left = left;
    pad_$set_full_window( out_stream, (short) 1, window_shape, status );

    pad_$set_auto_close( out_stream, (short) 1, true, status );

    size.x_size = 1024;  /* GPR will shrink to fit */
    size.y_size = 1024;

    /* Initialize the graphics primitives package */

    gpr_$init(gpr_$direct,
              out_stream,               /* "unit" */
              size,                     /* size of the initial bitmap */
              7,                        /* identifier of the heightest
                                           numbered bitmap plane */
              main_bitmap,              /* resulting bitmap descriptor */
              status);

    gpr_$set_auto_refresh( true, status );  /* all yours, DM. */

    gpr_$set_obscured_opt( gpr_$block_if_obs, status );

    gpr_$acquire_display( status ) ;

    /* set the new bitmap descriptor to be the 'current bitmap' */
    gpr_$set_bitmap(main_bitmap,status);
    gpr_$clear( gpr_$black, status );

    gpr_$release_display( status );
}

/* Write a scanline to the frame buffer using full 24 bit mode */
put_line_borrow(y)
int y;
{
    int i;
    gpr_$window_t dest_box;
    unsigned char *redptr, *grnptr, *bluptr;


    dest_box.window_base.x_coord = 0;
    dest_box.window_base.y_coord = 511 - y;
    dest_box.window_size.x_size = 511;
    dest_box.window_size.y_size = 1;

    redptr = rows[RLE_RED];
    if (hdr.ncolors == 3)
    { 
        grnptr = rows[RLE_GREEN];
        bluptr = rows[RLE_BLUE];
    }
    else	/* If only one channel, get all from same one  */
    {
        grnptr = rows[0];
        bluptr = rows[0];
    }
    for (i = hdr.xmin; i <= hdr.xmax; i++)
    {
	dest_pixels[i] = (gpr_$pixel_value_t) ((*redptr) << 16);
	dest_pixels[i] |= (gpr_$pixel_value_t) ((*grnptr) << 8);
	dest_pixels[i] |= (gpr_$pixel_value_t) (*bluptr);
	redptr++;  grnptr++; bluptr++;
    }
    gpr_$write_pixels( dest_pixels, dest_box, status );
    sysdebug;
}

/*
 * Map a 24 bit scanline to 4 bits through the dither matrix.
 */

put_line4( y )
int y;
{
    register unsigned char *r, *g, *b;
    register int i, col, row;
    gpr_$window_t dest_box;
    int n = hdr.xmax;

    r = rows[RLE_RED];
    if (hdr.ncolors == 3)
    { 
        g = rows[RLE_GREEN];
        b = rows[RLE_BLUE];
    }
    else
    {
        g = rows[0];
        b = rows[0];
    }

    for ( row = y % 16, col = 0, i = 0;
	 i <= n; i++, r++, g++, b++, col = ((col + 1) & 15) )
	dest_pixels[i] = DMAP(map[0][*r], col, row) +
	    		 DMAP(map[1][*g], col, row) * 2 +
			 DMAP(map[2][*b], col, row) * 4;

    dest_box.window_base.x_coord = hdr.xmin;
    dest_box.window_base.y_coord = hdr.ymax - y;
    dest_box.window_size.x_size = hdr.xmin + hdr.xmax + 1;
    dest_box.window_size.y_size = 1;

    gpr_$acquire_display( status ) ;
    gpr_$write_pixels( dest_pixels, dest_box, status ); 
    gpr_$release_display( status );

}

/*
 * Map a 24 bit scanline to 8 bits through the dither matrix.
 */

put_line8( y )
int y;
{
    register unsigned char *r, *g, *b;
    register int i, col, row;
    gpr_$window_t dest_box;
    int n = hdr.xmax;

    r = rows[RLE_RED];
    if (hdr.ncolors == 3)
    { 
        g = rows[RLE_GREEN];
        b = rows[RLE_BLUE];
    }
    else
    {
        g = rows[0];
        b = rows[0];
    }

    for ( row = y % 16, col = 0, i = 0;
	 i <= n; i++, r++, g++, b++, col = ((col + 1) & 15) )
	dest_pixels[i] = DMAP(map[0][*r], col, row) +
	    		 DMAP(map[1][*g], col, row) * 6 +
			 DMAP(map[2][*b], col, row) * 36 + COLMAP_OFFSET;

    dest_box.window_base.x_coord = hdr.xmin;
    dest_box.window_base.y_coord = hdr.ymax - y;
    dest_box.window_size.x_size = hdr.xmin + hdr.xmax + 1;
    dest_box.window_size.y_size = 1;

    gpr_$acquire_display( status ) ;
    gpr_$write_pixels( dest_pixels, dest_box, status ); 
    gpr_$release_display( status );

}

/*
 * Map a 24 bit scanline through to get dithered black and white
 */

put_linebw( y )
int y;
{
    register unsigned char *r, *g, *b;
    register int i, col, row;
    gpr_$window_t dest_box;
    int pixel;
    int n = hdr.xmax;
    gpr_$pixel_value_t B = 0, W = 1;

    r = rows[RLE_RED];
    if (hdr.ncolors == 3)
    { 
        g = rows[RLE_GREEN];
        b = rows[RLE_BLUE];
    }
    else
    {
        g = rows[0];
        b = rows[0];
    }

    if (bw_flag)
    {
        if (inverse_flag)
        {
	    B = 1; W = 0;		/* Swap meaning of Black and White */
        }

        for ( row = y % 16, col = 0, i = 0;
	     i <= n; i++, r++, g++, b++, col = ((col + 1) & 15) )
        {
	    /* Convert to BW (uses YIQ/percentage xformation) */
	    pixel = (35*map[0][*r] + 55*map[1][*g] + 10*map[2][*b]) / 100;
	    if (pixel < 0) pixel += 256;
	    dest_pixels[i] = ((DMAP(pixel, col, row) > 0) ? W : B);
	}
    }
    else
    {
        for ( row = y % 16, col = 0, i = 0;
	     i <= n; i++, r++, g++, b++, col = ((col + 1) & 15) )
        {
	    /* Convert to BW (uses YIQ/percentage xformation) */
	    pixel = (35*map[0][*r] + 55*map[1][*g] + 10*map[2][*b]) / 100;
	    if (pixel < 0) pixel += 256;
	    if (inverse_flag)
            {
	        pixel = ((~pixel) & 0xFF);		/* Swap meaning of Black and White */
            }
            if (four_flag)
            {
	        dest_pixels[i] = DMAP(pixel, col, row);
            }
            else
            {
		dest_pixels[i] = DMAP(pixel, col, row) + COLMAP_OFFSET;
	    }
	}
    }
    dest_box.window_base.x_coord = hdr.xmin;
    dest_box.window_base.y_coord = hdr.ymax - y;
    dest_box.window_size.x_size = hdr.xmin + hdr.xmax + 1;
    dest_box.window_size.y_size = 1;

    gpr_$acquire_display( status ) ;
    gpr_$write_pixels( dest_pixels, dest_box, status ); 
    gpr_$release_display( status );

}
