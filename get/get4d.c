/* 
 * get4d.c - Put RLE images on the Iris/4D display under the window manager.
 * 
 * Author:	Russell D. Fish (Based on getmex.c .)
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu May 26 20:49:11 1988
 * Copyright (c) 1988, University of Utah
 * 
 */
/* modified for Personal Iris with only 8 bit planes (no RGB mode)
 * as well as the other Iris displays.
 * Michael Hart - September 1991
 *    forced limitations - B&W display only, -G option forced
 */

#include <stdio.h>
#include <math.h>
#include "gl.h"
#include "device.h"
#include "rle.h"

#define MAX(i,j)   ( (i) > (j) ? (i) : (j) )
#define MIN(i,j)   ( (i) < (j) ? (i) : (j) )

/* Global variables. */
long window_number;			/* Window number from MAX. */
int x_size, y_size;			/* Size of image. */
int i;					/* General loop counter */
int dbg = 0;				/* Set if debug mode. */
int forkflg = 0;			/* Set if not to run in background. */
int bwflag = 0;				/* Set for greyscale output. */
int NoBorder = 0;			/* Turn off border */
int gt;			/* Whether we're using GT fast rectangle drawing. */
double disp_gamma = 1.0;		/* Gamma of Iris display. */
double img_gamma = 0.0;			/* Gamma of input image. */
short colmap[128][3];			/* Area to hold the old colormap */
long bitsavail;				/* 0 if not enough for RGB mode */

/* Window preferences. */
int posflag = 0, w_xpos, w_ypos, sizeflag = 0, w_xsize, w_ysize; 

unsigned long *rect_image;            /* GT Image data buffer pointer. */
unsigned char *rgb_image;             /* Non-GT Image data buffer pointer. */

rle_hdr		hdr;
rle_pixel **cmap;

/*****************************************************************
 * TAG( main )
 * 
 * Usage:
 *	get4d [-G][-S] [-p xpos ypos] [-f] [-D] [-w] [file]
 * Inputs:
 *	-D:	Debug mode: print input file as read.
 * 	-f:	Don't fork after putting image on screen.
 * 	-G:	GT mode: single fast rectangle, panning disabled.
 *		(GT mode is the default on GT, GTX, and Personal Iris 4Ds.)
 *	-S:	Slow mode: Allows resizing the window, and panning with the
 *		mouse.  (Slow mode is the default on non-GT 4Ds.)
 *	-g disp_gamma:	Gamma of the output display.
 *	-i image_gamma:	Gamma the image was calculated for.
 *	-I image_gamma:	Gamma of the image pixels (1.0 / (-i gamma)).
 *	-n:	No border will be drawn.
 *	-p xpos ypos:   Position of the lower left corner of the window.
 *	-s xsize ysize: Initial size of the window (slow mode only.)
 *
 *	-w:	Black & white: reduce color images to B&W before display.
 *		Advantage is that smoother shading can be achieved.
 *
 *	file:	Input Run Length Encoded file.  Uses stdin if not
 *		specified.
 * Outputs:
 * 	Puts image in a window on the screen.
 * Assumptions:
 * 	Input file is in RLE format.
 */

main(argc, argv)
char **argv;
{
    char *infname = NULL;
    FILE * infile = stdin;
    char vbuff[50];
    char *var;
    int gtflag = 0, slowflag = 0, gflag = 0, iflag = 0;

    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), NULL, 0 );

    /* Handle arguments. */
    if ( scanargs( argc, argv,
		   "% D%- f%- GS%- g%-disp_gamma!F iI%-image_gamma!F n%- \n\
	p%-xpos!dypos!d s%-xsize!dysize!d w%- file%s",
		   &dbg, &forkflg,
		   &gtflag,
		   &gflag, &disp_gamma,
		   &iflag, &img_gamma,
		   &NoBorder,
		   &posflag, &w_xpos, & w_ypos,
		   &sizeflag, &w_xsize, &w_ysize,
		   &bwflag,
		   &infname ) == 0 )
	exit( 1 );

    if ( gtflag )		/* Mode args override if specified. */
	gt = gtflag - 1;	/* -G->2 means yes GT, -S->1 means no GT. */
    else
    {
#ifndef _IBMR2
	/* See if we`re on a GT.  For this purpose, a Personal Iris is a GT. */
	gversion( vbuff );	/* String like "GL4DGTX-3.1". */
	gt = strncmp( vbuff+4, "GT", 2 ) == 0 ||
	     strncmp( vbuff+4, "PI", 2 ) == 0;
#else
	gt = 1;
#endif
    }
    
    /* Diddle with gammas. */
    if ( iflag == 2 )		/* -i flag. */
	if ( img_gamma != 0.0 )	/* Paranoid. */
	    img_gamma = 1.0 / img_gamma;

#ifndef _IBMR2
    /* Look at home for .gamma file. */
    if ( !gflag )
    {
	char *var = getenv( "HOME" );
	char buf[BUFSIZ];
	FILE *gamfile;
	float flt_gam = 0;	/* Just in case of 68000 iris botch. */

	if ( var != NULL && *var != '\0' )
	{
	    sprintf( buf, "%s/.gamma", var );
	    if ( (gamfile = fopen( buf, "r" )) != NULL )
	    {
		fscanf( gamfile, "%f", &flt_gam );
		if ( flt_gam != 0 )
		    disp_gamma = 2.4/flt_gam;/* SGI displays have gamma 2.4. */
		fclose( gamfile );
	    }
	}
    }
#endif
    /* check machine graphics capability */
    bitsavail = getgdesc(GD_BITS_NORM_SNG_GREEN); /* Can it support RGB mode */
    if(bitsavail==0)
    {
                /* set configuration to force reading in B&W and -G ode disp */
        bwflag = 1;
        gt = 1;
    }		
    infile = rle_open_f(hdr.cmd, infname, "r");
    get_pic( infile, infname );	/* Read image and make a window for it. */
    if(bitsavail==0)            /* modify gey levels if necessary */
        convertgreylevels();
    update_pic();		/* Keep drawing the window. */
}

/* 
 * Read an image from the input file and display it.
 */
get_pic( infile, infname )
FILE * infile;
char * infname;
{
    register int i, y;
    int ncolors;
    unsigned char *scan[3];

    /*
     * Read setup info from file. 
     */
    rle_hdr_init( &hdr );
    rle_names( &hdr, hdr.cmd, infile, 0 );
    hdr.rle_file = infile;
    if ( rle_get_setup( &hdr ) < 0 )
    {
	fprintf(stderr, "%s: Error reading setup information from %s\n",
		hdr.cmd, infname ? infname : "stdin");
	exit(1);
    }

    if ( dbg )
	rle_debug( 1 );

    /* We`re only interested in R, G, & B */
    RLE_CLR_BIT(hdr, RLE_ALPHA);
    for (i = 3; i < hdr.ncolors; i++)
	RLE_CLR_BIT(hdr, i);
    ncolors = hdr.ncolors > 3 ? 3 : hdr.ncolors;

    /* Do nicer b&w rendering if only one color channel in input. */
    if ( ncolors == 1 && hdr.ncmap <= 1 )
	bwflag = 1;

    /*
     * Compute image size and allocate storage for colormapped image. 
     */
    x_size = (hdr.xmax - hdr.xmin + 1);
    y_size = (hdr.ymax - hdr.ymin + 1);
    if ( gt )
      rect_image = (unsigned long *) malloc(x_size * y_size * sizeof( long ));
    else
      rgb_image = (unsigned char *) malloc(x_size * y_size * 3 * sizeof( char ));

    /*
     * Set up for rle_getrow.  Pretend image x origin is 0. 
     */
    for (i = 0; i < 3; i++)
	scan[i] = (unsigned char *) malloc(x_size);
    hdr.xmax -= hdr.xmin;
    hdr.xmin = 0;

    cmap = buildmap( &hdr, 3, img_gamma, disp_gamma );

    /* For each scan line, pack RGBs into the image memory. */
    while ((y = rle_getrow(&hdr, scan)) <= hdr.ymax)
    {
	switch ( ncolors )
	{
	case 1:
	    /* Colormapped image */
	    for (i = 0; i < x_size; i++) {
		scan[2][i] = cmap[2][scan[0][i]];
		scan[1][i] = cmap[1][scan[0][i]];
		scan[0][i] = cmap[0][scan[0][i]];
	    }
	    break;

	case 2:
	    /* Weird image. */
	    for (i = 0; i < x_size; i++) {
		scan[2][i] = cmap[2][scan[1][i]];
		scan[1][i] = cmap[1][scan[1][i]];
		scan[0][i] = cmap[0][scan[0][i]];
	    }
	    break;
	case 3:
	    /* Normal image. */
	    for (i = 0; i < x_size; i++) {
		scan[2][i] = cmap[2][scan[2][i]];
		scan[1][i] = cmap[1][scan[1][i]];
		scan[0][i] = cmap[0][scan[0][i]];
	    }
	    break;
	}
	if ( bwflag )
	{
	    if (ncolors == 1)
		rgb_to_bw (scan[0], scan[1], scan[2], scan[0], x_size );
	    else
	        rgb_to_bw( scan[0], scan[1], scan[ncolors - 1],
		    scan[0], x_size );
	    /* Note: pack_scanline only uses channel 0 for B&W */
	}

	if ( gt )
	    pack_rect_scanline( scan, x_size,
		&rect_image[(y - hdr.ymin) * x_size] );
	else
	    pack_rgb_scanline( scan, x_size,
		&rgb_image[(y - hdr.ymin) * x_size * 3] );
    }

    /*
     * Free temp storage 
     */
    for (i = 0; i < 3; i++)
	free(scan[i]);

    /* Size flag can cut down the window size, except in GT rectangle mode. */
    w_xsize = sizeflag && !gt ? MIN( w_xsize, x_size ) : x_size;
    w_ysize = sizeflag && !gt ? MIN( w_ysize, y_size ) : y_size;

    /* Window should be completely on the screen in GT rectangle mode. */
    if ( posflag && gt )
    {
	w_xpos = MIN( w_xpos, XMAXSCREEN - w_xsize );
	w_ypos = MIN( w_ypos, YMAXSCREEN - w_ysize);
    }

    w_xpos = MAX( 0, w_xpos );	/* Be positive. */
    w_ypos = MAX( 0, w_ypos );

    /* Register our preferences for the window size and location. */
    if ( posflag )
	prefposition( w_xpos, w_xpos + w_xsize - 1,
		      w_ypos, w_ypos + w_ysize - 1 );
    else if ( sizeflag || gt )
        /* Have to keep the window full size to use the fast pixel commands. */
          prefsize( w_xsize, w_ysize );
    else
        maxsize( x_size, y_size );

    /*
     * Get a window of the right size (user positions it with the mouse). 
     */
#ifndef _IBMR2
    if ( forkflg ) foreground();	/* Don`t fork. */
#endif
    if ( NoBorder ) noborder();
    window_number = winopen( hdr.cmd );
    if ( infname ) wintitle( infname );

    /* Loosen the constraints once the window is created. */
    if ( !gt )
	maxsize( x_size, y_size );
    winconstraints();

    if(bitsavail==0)
    {		/* only color map available so make a grey scale */
        for(i=0;i<=127;i++)
	{
		getmcolor((i+128),&colmap[i][0],&colmap[i][1],&colmap[i][2]);
		mapcolor((i+128),(i*2),(i*2),(i*2));
	}
    }
    else /* do the normal thing */
        RGBmode();
    gconfig();

    qdevice( ESCKEY );
    qdevice ( WINQUIT );
    qdevice( REDRAW );
    unqdevice( INPUTCHANGE );	/* We don`t pay attention to these. */

    if ( !gt )		/* Disable panning when in gt rectangle mode. */
    {
	qdevice( LEFTMOUSE );	/* Pan the image under mouse control. */
	qdevice( LEFTALTKEY );	/* Reset panning. */
	qdevice( RIGHTALTKEY );
	qdevice( F9KEY );
    }

    /* There was a redraw event sent when the window was created,
     * but we weren`t listening for them yet.
     */
    qenter( REDRAW, window_number );
}

/* 
 * Track events & redraw image when necessary.
 */
update_pic()
{
    short data;
    long event;
    long window_x_size, window_y_size, window_x_origin, window_y_origin;
    int x_min, x_max, y_min, y_max, x_start, y_start, x_end, y_end, x_len;
    long x_origin, y_origin, new_x_center, new_y_center;
    int x_center, y_center, saved_x_center, saved_y_center;

    register int y;
    register unsigned char *y_ptr;

    /* Looking at the center, at first. */
    x_center = saved_x_center = x_size / 2;
    y_center = saved_y_center = y_size / 2;

    /* Redraw the window when necessary. */
    while ( TRUE )
    {
        event = qread( &data );
# ifdef DEBUG
	printf( "event %d, data %d\n", event, data );
#endif	
	switch ( event )
	{
 	    case ESCKEY:
            case WINQUIT:
                if(bitsavail==0) /* restore colormap */
                    for(i=0;i<=127;i++)
                        mapcolor((i+128),colmap[i][0],colmap[i][1],colmap[i][2]);
 		gexit();
 		exit(0);
 		break;
	    case REDRAW:
		winset( window_number );
		reshapeviewport();

 		if ( gt )
		{
		    /* On a GT, just blast out the whole rectangle.  If the
		     * origin is off the screen to the left, it kills the
		     * window server.  Could duplicate the slow output logic
		     * below, but avoid the complication for now...
		     */
		    getorigin( &x_origin, &y_origin );
		    if ( x_origin < 0 )
		    {
			RGBcolor( 128, 128, 128 );	/* Punt. */
			clear();
		    }
		    else
			lrectwrite( 0, 0, x_size-1, y_size-1, rect_image );
		}
		else
		{
		    /* Do panning in a resizable window.  (Slow mode.) */
		    RGBcolor( 128, 128, 128 );
		    clear();
		
		    /* Lower left corner of screen, in image coordinates.
		     * (Keep the center of the image in the center of the
		     * window.)
		     */
		    getsize( &window_x_size, &window_y_size );
		    x_min = x_center - window_x_size/2;
		    x_max = x_min + (window_x_size-1);
		    y_min = y_center - window_y_size/2;
		    y_max = y_min + (window_y_size-1);

		    /* Coordinate bounds have half a pixel added all around. */
		    ortho2( x_min - .5, x_max + .5, y_min - .5, y_max + .5 );

		    /* Draw just the part of the image in the window. */
		    x_start = MAX( x_min, 0 );
		    y_start = MAX( y_min, 0 );
		    x_end = MIN( x_max, x_size-1 );
		    y_end = MIN( y_max, y_size-1 );
		    x_len = x_end - x_start + 1;

		    /* Dump the scanlines.  Check once in a while for another
		     * redraw event queued up, and quit early if one is seen.
		     */
		    y_ptr = rgb_image + y_start*x_size*3 + x_start;
		    for ( y = y_start;
			  y <= y_end && (y%16 != 0 || qtest() != REDRAW);
			  y++, y_ptr += x_size * 3 )
		    {
			cmov2i( x_start, y );
			writeRGB( x_len,
				  y_ptr, y_ptr + x_size, y_ptr + x_size * 2 );
		    }
		}
		break;

	    /* Alt key - Reset viewing to look at the center of the image.
	     * Shift-Alt - Restores a saved view center.
	     * Control-alt - Saves the current view center for Shift-Setup.
	     * F9 is the same as the Alt keys.
	     */
	    case LEFTALTKEY:
	    case RIGHTALTKEY:
	    case F9KEY:
		if ( data == 1 )	/* Ignore button up events. */
		{
		    if ( getbutton(RIGHTSHIFTKEY) || getbutton(LEFTSHIFTKEY) )
		    {
			x_center = saved_x_center;	/* Restore. */
			y_center = saved_y_center;
			qenter( REDRAW, window_number );
		    }
		    else if ( getbutton(CTRLKEY) )
		    {
			saved_x_center = x_center;	/* Save. */
			saved_y_center = y_center;
		    }
		    else
		    {
			x_center = x_size / 2;		/* Reset. */
			y_center = y_size / 2;
			qenter( REDRAW, window_number );
		    }

		}
		break;

	    /* Pan a point picked with the left mouse button to the center
	     * of attention.  Beep if cursor is not on the image.
	     */
	    case LEFTMOUSE:
		if ( data == 1 ) 	/* Ignore button up events. */
		{
		    getorigin( &x_origin, &y_origin );
		    new_x_center = getvaluator( MOUSEX ) - x_origin + x_min;
		    new_y_center = getvaluator( MOUSEY ) - y_origin + y_min;
		    if ( new_x_center >= x_start &&
			 new_x_center <= x_end &&
			 new_y_center >= y_start &&
			 new_y_center <= y_end )
		    {
			x_center = new_x_center;
			y_center = new_y_center;
			qenter( REDRAW, window_number );
		    }
		    else
			ringbell();
		}
		break;
	}
    }
}

/*
 * Pack a scanline into a vector of RGB longs.
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	line:		Pointer to output buffer for packed color data.
 */
pack_rect_scanline( rgb, n, line )
unsigned char *rgb[3];
int n;
long *line;
{
    register int i;
    register long *dest = line;

    if ( !bwflag )			/* Color display. */
    {
	register unsigned char *r, *g, *b;

	for ( i = 0, r = rgb[0], g = rgb[1], b = rgb[2];
	      i < n; i++, r++, g++, b++ )
	    /* Combine 3 8-bit colors into a long. */
	    *dest++ = *r + (*g<<8) + (*b<<16);
    }
    else				/* Gray scale display. */
    {
	register unsigned char *bw;

	for ( i = 0, bw = rgb[0]; i < n; i++, bw++ )
	    *dest++ = *bw + (*bw<<8) + (*bw<<16);
    }
}

/*
 * Pack a scanline into an RGB trio of vectors of bytes.
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	line:		Pointer to output buffer for packed color data.
 */
pack_rgb_scanline( rgb, n, lines )
unsigned char *rgb[3];
int n;
unsigned char *lines;
{
    int chnl;
    register int i;
    register unsigned char *src, *dest;

    for ( chnl = 0, dest = lines; chnl < 3; chnl++ )
    {
	src = rgb[ bwflag ? 0 : chnl ];	/* Use just channel 0 for greyscale. */
	for ( i = 0; i < n; i++ )
	    *dest++ = *src++;
    }	    
}

/*
 * This routine is called only if the color map is required.  It converts
 * the 8 bit grey scale levels to 7 bit grey scale levels starting at 128
 * and going through to 255.  We can't use the full 256 grey levels to 
 * display the image because the windows and text of other applications
 * can't be seen if they are remapped to a grey scale.
 */
convertgreylevels(void)
{
    for(i=0;i<(x_size*y_size);i++)
        (*(rect_image+i)) = (((*(rect_image+i))&255)>>1)|128;
}


