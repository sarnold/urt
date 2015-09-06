/* 
 * getiris.c - Whoop up an rle with 24 bits ( no mex ).
 * 
 * Author:	Glenn McMinn
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Feb  3 1987
 * Copyright (c) 1987 Glenn McMinn
 * 
 */


#define MIN(a,b) ( a<b ? a : b)

#include "gl.h"
#include "device.h"
#include "stdio.h"
#include "rle.h"

main(argc,argv)
int argc;
char **argv;
{
    	rle_hdr hdr;
	char *infname;
        int x_len, y_len;
	int i;
	unsigned char **scan;
	Device val;

	hdr = *rle_hdr_init( (rle_hdr *)NULL );

	if ( scanargs( argc, argv, "% infile%s\n(\
\tDisplay on SGI not running a window manager.)",
		       &infname ) == 0 )
	    exit( 1 );

	rle_names( &hdr, cmd_name( argv ), infname, 0 );

	/* This program runs without mex running. */
	if ( ismex())
	{
		fprintf( stderr, "%s:  can't run under mex!\n", hdr.cmd);
		exit(0);
	}
	ginit();

	/* Turn off cursor so that picture doesn't have a glich. */
	cursoff();
	RGBmode();
	gconfig();
	RGBcolor(0, 0, 0);
	clear();

	/* Setup mouse buttons so that they are queued. */
	qdevice( LEFTMOUSE );
	qdevice( RIGHTMOUSE );
	qdevice( MIDDLEMOUSE );
	qreset();

	/* Take input from file argument or stdin. */ 	
	hdr.rle_file = rle_open_f(hdr.cmd, infname, "r");
	rle_get_setup( &hdr );

	x_len = hdr.xmax - hdr.xmin + 1;
	y_len = hdr.ymax - hdr.ymin + 1;
	hdr.xmax -= hdr.xmin;
	hdr.xmin = 0;

	/* Grab a scanline. */
	scan = (unsigned char **) malloc( (hdr.ncolors +
				       hdr.alpha) *
				      sizeof( unsigned char * ) );
	for ( i = 0; i < hdr.ncolors + hdr.alpha; i++ )
	    scan[i] = (unsigned char *)malloc( x_len );

	if ( hdr.alpha )
	{
	    scan++;
	}

	/* Display each scanline. */
        for ( i = 0; i < MIN(y_len,768) ; i ++)
	{
		rle_getrow(&hdr, scan );

		cmov2i(0, i);
		writeRGB(x_len, scan[0], scan[1], scan[2]);
	}

	/* Wait for a mouse button push. */
	qread( &val);

	/* Set the iris back up so that it is not scrogged. */
	singlebuffer();
	gconfig();
	clear();
	gexit();
}
