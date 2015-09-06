/* 
 * getren.c - getren - Display images on the HP Renaissance (98720A)
 * 
 * Author:	John W. Peterson & Glenn McMinn
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Aug 18 1987
 * Copyright (c) 1987, John W. Peterson
 *
 * TODO:
 *  - Should warn you if configuration is not sufficient (< 24 bits)
 */

#define MIN(a,b) ( a<b ? a : b)
#include <stdio.h>
#include <starbase.c.h>
#include "rle.h"

int display_flag = FALSE;
int driver_flag = FALSE;
char *display_name = "/dev/crt";
char *driver_name  = "hp98721";

#define RASTERHEIGHT 1024	/* Calling inquire_sizes is just too tedious */

main(argc,argv)
int argc; char *argv[];
{
    int fildes;
    int x_len, y_len, x_off = 0, y_off = 0;
    int i, j, map_offset ;
    rle_hdr hdr;
    unsigned char **scan;
    char * filename = NULL;
    int over_flag = 0;
    int pos_flag = 0;
    float tmp;
    float colors[256][3];
    gescape_arg arg1, arg2;

    if (! scanargs( argc, argv,
	"% pP%-xpos!dypos!d O%- d%-display!s x%-driver!s infile%s",
		    &pos_flag, &x_off, &y_off, &over_flag,
		    &display_flag,&display_name,
		    &driver_flag,&driver_name,
		    &filename ))
	exit( -1 );

    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), filename, 0 );

    if (! over_flag)
	over_flag = INIT;

    if (( fildes = gopen(display_name, OUTDEV,
			 driver_name, over_flag )) < 0)
	fprintf(stderr, "%s: can't open %s\n", hdr.cmd, display_name);

    hdr.rle_file = rle_open_f(hdr.cmd, filename, "r");

    rle_get_setup_ok( &hdr, NULL, NULL );

    /* So we see the whole FB. */
    clip_indicator( fildes, CLIP_OFF );

    arg1.i[0] = FALSE;
    gescape( fildes, SWITCH_SEMAPHORE, &arg1, &arg2 );

    /* This version assumes a full 24-bit machine */
    shade_mode( fildes, CMAP_FULL, 0 );

    if ( hdr.ncmap )	/* Use colormap in file */
    {
	map_offset = 1 << hdr.cmaplen;
	if (hdr.ncmap == 3)
	{
	    for (i = 0; i < 256; i++)
	    {
		colors[i][0] = (float) ((hdr.cmap[i] >>8) & 0xFF)
		                        / 255.0;
		colors[i][1] = (float) ((hdr.cmap[i+map_offset] >>8)
					& 0xFF) / 255.0;
		colors[i][2] = (float) ((hdr.cmap[i+2*map_offset]>>8)
					& 0xFF) / 255.0;
	    }
	}
	else
	if (hdr.ncmap == 1)
        {
	    for (i = 0; i < 256; i++)
	    {
		tmp = (float) ((hdr.cmap[i] >> 8) * 0xFF);
		colors[i][0] = tmp;
		colors[i][1] = tmp;
		colors[i][2] = tmp;
	    }
	}
	else
	{
	    fprintf(stderr, "%s: Sorry, don't grock %d channel color maps\n",
		    hdr.cmd, hdr.ncmap);
	    exit(-1);
	}
    }
    else
    {
	/* Set the color map to default */
	for (i = 0; i < 256; i++)
	{
	    tmp = (float) i / 255.0;
	    colors[i][0] = tmp;
	    colors[i][1] = tmp;
	    colors[i][2] = tmp;
	}
    }
    define_color_table( fildes, 0, 256, colors );

    x_len = hdr.xmax - hdr.xmin + 1;
    y_len = hdr.ymax - hdr.ymin + 1;
    if (pos_flag)
    {
	if (pos_flag == 1)	/* -P, incremental position */
	{
	    x_off = hdr.xmin + x_off;
	    y_off += hdr.ymin;
	}
    }
    else
    {
	x_off = hdr.xmin;
	y_off = hdr.ymin;
    }
	
    y_off = RASTERHEIGHT - y_off - 1; /* Invert origin */

    /* This gets rid of the left margin slop in the scanline. */
    hdr.xmax -= hdr.xmin;
    hdr.xmin = 0;

    if (rle_row_alloc( &hdr, &scan ) < 0)
	RLE_CHECK_ALLOC( hdr.cmd, 0, "image memory" );

    /* Display each scanline. */

    y_len = MIN( RASTERHEIGHT-1, y_len );
    for ( i = 0; i < y_len ; i ++)
    {
	rle_getrow(&hdr, scan );

	for (j = RLE_RED; j < hdr.ncolors; j++)
	{
	    /* HP numbers channels backwards from URT */
	    bank_switch( fildes, 2-j, 0 );
	    dcblock_write( fildes, x_off, y_off - i,
			  x_len, 1, scan[j], 0 );
	}

	/* KLUDGE for black and white.  */
	while (j <= RLE_BLUE)
	{
	    bank_switch( fildes, 2-j, 1 );
	    dcblock_write( fildes, x_off, y_off - i,
			  x_len, 1, scan[0], 0 );
	    j++;
	}
    }
}

