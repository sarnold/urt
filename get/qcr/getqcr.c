/* 
 * getqcr.c - Image display for the Matrix QCR-Z
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Jan 20 1988
 * Copyright (c) 1988, University of Utah
 *
 * BUGS
 *  verbose flag should turn on "reading red..." type messages.
 *  Should handle single channel images as black and white.
 */

#include <stdio.h>
#include "qcr.h"
#include "rle.h"

main (argc, argv)
int argc;
char ** argv;
{
    rle_hdr hdr;
    rle_pixel * camera_data = NULL, * cdptr;
    rle_pixel ** rows;
    char * filename;
    int color = 0;
    int y, i, ysize;
    int verbose_flag = 0, double_flag = 0, exposures = 1, exp_flag = 0;
    int center_flag = 0, pos_flag = 0, fourK = 0, xstart = 0, ystart = 0;

    if (! scanargs( argc, argv,
		   "% v%- d%- c%- f%- p%-xpos!dyposn!d e%-num!d infile!s",
		   &verbose_flag, &double_flag, &center_flag,
		   &fourK, &pos_flag, &xstart, &ystart,
		   &exp_flag, &exposures, &filename ))
	exit(-1);

    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), filename );

    hdr.ncolors = 1;	/* Assume at least one */
    init_qcr( verbose_flag );

    if (double_flag)
	exposures = 2;

    while( color < hdr.ncolors )
    {
	hdr.rle_file = rle_open_f( hdr.cmd, filename, "r" );
	if ( hdr.ncolors > 1 &&
	     hdr.rle_file == stdin &&
	     fseek( hdr.rle_file, 0L, 0 ) < 0 )
	{
	    fprintf( "Can't pipe color data to %s.\n", hdr.cmd );
	    exit( -1 );
	}

	rle_get_setup_ok( &hdr, NULL, NULL );

	if (! pos_flag)
	{
	    xstart = hdr.xmin;
	    ystart = hdr.ymin;
	}

	hdr.xmax -= hdr.xmin;
	hdr.xmin = 0;
	ysize = (hdr.ymax - hdr.ymin + 1);

	if (! camera_data)	/* Only allocate once */
	{
	    rows = (rle_pixel **) malloc( hdr.ncolors
					 * sizeof( rle_pixel * ));
	    camera_data = (rle_pixel *)
		malloc( (hdr.xmax + 1) * ysize);

	    RLE_CHECK_ALLOC( hdr.cmd, rows && camera_data, "image memory" )
	}

	/* Only read one color at a time, never alpha */
	for (i = -hdr.alpha; i < hdr.ncolors; i++)
	    RLE_CLR_BIT( hdr, i );
	RLE_SET_BIT( hdr, color );

	rows[color] = camera_data;

	if (center_flag)
	    set_up_qcr( hdr.xmax + 1, ysize, ysize, 0 );
	else
	    set_up_qcr_nc( xstart, ystart,
			   hdr.xmax + 1, ysize, fourK );

	for (y = hdr.ymin; y <= hdr.ymax; y++)
	{
	    rle_getrow( &hdr, rows );
	    rows[color] = &(rows[color][hdr.xmax+1]);
	}

	for ( i = 0; i < exposures; i++)
	    send_pixel_image( color, camera_data,
			      (hdr.xmax + 1) * ysize);

	fclose( hdr.rle_file );
	color++;
    }
    exit( 0 );
}
