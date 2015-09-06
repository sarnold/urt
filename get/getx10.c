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
 * getx10.c - Put RLE images on X display.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Feb 20 1986
 * Copyright (c) 1986, University of Utah
 * 
 */
#ifndef lint
static char rcs_ident[] = "$Id: getx10.c,v 3.0.1.1 1992/01/28 18:12:35 spencer Exp $";
#endif

#include <stdio.h>
#include <math.h>
#include <X/Xlib.h>
#include "rle.h"

/* Most that can be sent to X in one chunk */
#define MAXSEND	65535			/* 64K */

/*
 * Basic magic square for dithering
 */
int dm16[16][16];

/* define arrow cursor for zoom mode */
#define arrow_width 16
#define arrow_height 16
#define arrow_x_hot 4
#define arrow_y_hot 1
static short arrow_bits[] = {
   0x0000, 0x0010, 0x0030, 0x0070,
   0x00f0, 0x01f0, 0x03f0, 0x07f0,
   0x0ff0, 0x01f0, 0x03b0, 0x0310,
   0x0700, 0x0600, 0x0600, 0x0000};
#define arrow_mask_width 16
#define arrow_mask_height 16
static short arrow_mask_bits[] = {
   0x0018, 0x0038, 0x0078, 0x00f8,
   0x01f8, 0x03f8, 0x07f8, 0x0ff8,
   0x1ff8, 0x1ff8, 0x07f8, 0x07b8,
   0x0f98, 0x0f00, 0x0f00, 0x0f00};
Cursor arrow_curs;

/* 
 * Color map, gamma correction map, and lookup tables 
 */
Color colmap[256];
int gammamap[256];
rle_pixel ** in_cmap;
int modN[256], divN[256];
int bwflag = 0;			/* if non zero, dither in B&W
				 * Value of 2 means 1-bit system
				 */

/*
 * Number of color map levels, (square and cube), and number of gradations
 * per level.
 */
int levels = 0, levelsq, levelsc;

/* 
 * Global variables
 */
rle_hdr hdr;
double disp_gam = 2.5;		/* default gammas for display and image */
double img_gam = 1.0;
int iflag = 0;			/* flag to tell if gamma was on command line */
int setbg = 0;			/* set root background to image? */
int mapflg = 0;			/* flag to just load color map */
int usemap = 0;			/* Use given color map, don't dither */
int forkflg = 0;		/* No fork allows zoom info (middle button) */
int zoomflg = 0;		/* build zoom window */

Display * dpy;
Window fbwin, iconwin, zoomwin;
int iconfact, iconrow, iconscan, iconbyte;
int zoomfact = 8;
int zoom_x_dim = 15;
int zoom_y_dim = 15;
int zoom_x_center, zoom_y_center;
int nscan, nrow;			/* size of window */
int nbyte;			/* size of bitrow for 1-bit displays */

int dbg = 0;			/* set if debug mode */

unsigned char *buffer, *iconbuf;	/* data storage pointers */

/*****************************************************************
 * TAG( main )
 * 
 * Usage:
 *	getx10 [-{bB}] [-z] [-m] [-f] [-p] [-D] [-d display]
 *		[-= window-geometry] [-{iI} gamma] [-g gamma] [file]
 * Inputs:
 *	-b:		Set the root window background to the resultant image.
 *	-B:		Set the root background, but don't display the image
 *			in a separate window.
 *	-z:		Create Zoom window also.  Any button in image recenters
 *			zoom.  Drag in image resizes zoom window to enclose
 *			region.  Left in zoom window decreases zoom factor.
 *			Right increases zoom factor.  Middle prints position
 *			info to the terminal, but only if -f is on.
 *	-m:		Just load color map and exit.
 * 	-f:		Don't fork after putting image on screen.
 *	-p:		Don't use off-screen memory to speed up redraw.
 *	-D:		Debug mode: print input file as read.
 *	-w:		Black & white: reduce color images to B&W before
 *			display.  Advantage is that smoother shading can
 *			be achieved.
 *	-c:		Use colors specified in color map.  Will try to load
 *			entire color map.  No dithering will be done
 *			when this option is specified.  Actual number
 *			of entries in color map may be given as a
 *			picture comment:
 *			color_map_length=<number of entries in color map>.
 *	-d display:	Specify display name.
 *	-= window_geometry:
 *			Specify window geometry (but min size set by file).
 *	-i gamma:	Specify gamma of image. (default 1.0)
 *	-I gamma:	Specify gamma of display image was computed for.
 * 	getx10 will also read picture comments from the input file to determine
 *			the image gamma.  These are
 *	image_gamma=	gamma of image (equivalent to -i)
 *	display_gamma=	gamma of display image was computed for.
 *			Command line arguments override values in the file.
 *		
 *	-g gamma:	Specify gamma of display. (default 2.5)
 *	file:		Input Run Length Encoded file. Uses stdin if not
 *			specified.
 * Outputs:
 * 	Puts image on screen.
 * Assumptions:
 * 	Input file is in RLE format.
 * Algorithm:
 *	[None]
 */

main(argc, argv)
char **argv;
{
    char ** infnames = NULL, *infname = NULL, *display_name = NULL,
	* window_geometry = NULL;
    FILE * infile = stdin;
    int nfile = 0, use_pix = 0,
	dflag = 0, gflag = 0, wflag = 0;

    if ( scanargs( argc, argv,
		   "% Bb%- m%- f%- p%- cWw%- D%- n%-levels!d d%-display!s \n\
\t=%-window-geometry!s Ii%-gamma!F g%-gamma!F z%- file%s",
		   &setbg, &mapflg, &forkflg, &use_pix, &bwflag, &dbg,
		   &levels, &levels,
		   &dflag, &display_name,
		   &wflag, &window_geometry,
		   &iflag, &img_gam,
		   &gflag, &disp_gam,
		   &zoomflg,
		   &infname ) == 0 )
	exit( 1 );

    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), infname );

    use_pix = ! use_pix;
    if ( setbg & !use_pix )
    {
	fprintf( stderr, "Can't specify -p with -b, -p ignored\n" );
	use_pix = 1;
    }
    if ( setbg == 2 )
	forkflg = 1;

    if ( iflag == 1 )		/* -i */
	img_gam = 1.0 / img_gam;

    if ( bwflag == 4 )		/* -c */
    {
	bwflag = 0;
	usemap = 1;
    }

    /* Would like to be able to use multiple files, but haven't
     * figured how to get X to let us.  So, use this kludge for now.
     */
    if ( infname != NULL )
    {
	infnames = &infname;
	nfile = 1;
    }

    /* 
     * For each file, display it.
     */
    do {
	if ( nfile > 0 )
	{
            infile = rle_open_f(hdr.cmd, *infnames, "r");
	    get_pic( infile, *infnames, display_name,
		     window_geometry );
	    fclose( infile );
	    infnames++;
	    nfile--;
	}
	else
	    get_pic( stdin, NULL, display_name,
		     window_geometry );
	if ( ! forkflg )
	{
	    if ( fork() == 0 )
	    {
		/* 
		 * Get rid of std fds so rshd will go
		 * away.
		 */
		close( 0 );
		close( 1 );
		close( 2 );
		update_pic( use_pix );
		break;
	    }
	}
	else
	{
	    update_pic( use_pix );
	}
    } while ( nfile > 0 );
    exit( 0 );
    /*	XCloseDisplay( dpy );*/
}

/* 
 * Read an image from the input file and display it.
 */
get_pic( infile, infname, display_name, window_geometry )
FILE * infile;
char * infname;
char * display_name;
char * window_geometry;
{
    register int    i,
                    y;
    int		    ncolors,
		    xmin_original;
    unsigned char  *scan[3];

    /*
     * Read setup info from file. 
     */
    hdr.rle_file = infile;
    if ( !mapflg || usemap )
	rle_get_setup_ok(&hdr, NULL, NULL);

    if ( dbg )
	rle_debug( 1 );

    /* We're only interested in R, G, & B */
    RLE_CLR_BIT(hdr, RLE_ALPHA);
    for (i = 3; i < hdr.ncolors; i++)
	RLE_CLR_BIT(hdr, i);
    ncolors = hdr.ncolors > 3 ? 3 : hdr.ncolors;

    /*
     * Open display first time through. 
     */
    if (dpy == NULL)
    {
	dpy = XOpenDisplay(display_name);
	if (dpy == NULL)
	{
	    fprintf(stderr, "%s: Can't open display %s\n",
		    hdr.cmd, display_name ? "" : display_name);
	    exit(1);
	}
	if ( DisplayPlanes() == 1 )	/* b&w display */
	    bwflag = 2;
    }

    /* If no image gamma on command line, check comments in file */
    if ( ! iflag )
    {
	char * v;
	if ( (v = rle_getcom( "image_gamma", &hdr )) != NULL )
	{
	    img_gam = atof( v );
	    /* Protect against bogus information */
	    if ( img_gam == 0.0 )
		img_gam = 1.0;
	    else
		img_gam = 1.0 / img_gam;
	}
	else if ( (v = rle_getcom( "display_gamma", &hdr )) != NULL )
	{
	    img_gam = atof( v );
	    /* Protect */
	    if ( img_gam == 0.0 )
		img_gam = 1.0;
	}
    }

    /*
     * Set up the color map. 
     */
    /* Input map, at least 3 channels */
    in_cmap = buildmap( &hdr, 3, img_gam, 1.0 );
    /* Get X color map */
    if ( usemap )
	load_x_map();
    else
	init_color();

    if ( mapflg )
	exit( 0 );

    /*
     * Compute image size and allocate storage. 
     */
    nrow = (hdr.xmax - hdr.xmin + 1);
    if ( bwflag == 2 )
	nbyte = ((nrow + 15) / 16) * 2;	/* 1 bit display */
    else
	nbyte = nrow;
    nscan = (hdr.ymax - hdr.ymin + 1);
    buffer = (unsigned char *) malloc(nbyte * nscan);

    /*
     * Icon wants to be about 50 x 50.  Figure out how much smaller than the
     * image this is. 
     */
    iconfact = nrow / 50;
    if (iconfact < nscan / 50)
	iconfact = nscan / 50;
    if ( iconfact == 0 )
	iconfact = 1;
    iconrow = (1 + nrow / iconfact);
    iconscan = (1 + nscan / iconfact);
    if ( bwflag == 2 )
    {
	iconbuf = (unsigned char *)
	    malloc(BitmapSize( (1 + nrow / iconfact),
			       (1 + nscan / iconfact) ));
	iconbyte = 2 * ((iconrow + 15) / 16);
    }
    else
    {
	iconbuf = (unsigned char *) malloc((1 + nrow / iconfact) *
					   (1 + nscan / iconfact));
	iconbyte = iconrow;
    }

    /*
     * Set up for rle_getrow.  Pretend image x origin is 0. 
     */
    for (i = 0; i < 3; i++)
	scan[i] = (unsigned char *) malloc(nrow);
    hdr.xmax -= hdr.xmin;
    xmin_original = hdr.xmin;
    hdr.xmin = 0;

    /*
     * Get a window of the right size (user positions it with the mouse). 
     */
    if ( setbg < 2 )
	create_window(nrow, nscan, window_geometry);

    /*
     * For each scan line, dither it and display. 
     */
    while ((y = rle_getrow(&hdr, scan)) <= hdr.ymax)
    {
	if ( bwflag && ncolors > 1 )
	{
	    map_rgb_to_bw( scan[0], scan[1], scan[ncolors - 1], scan[0],
			   in_cmap, nrow );
	    /* Note: map_scanline only uses channel 0 for B&W */
	}
	else if ( bwflag )
	    for ( i = 0; i < nrow; i++ )
		scan[0][i] = in_cmap[0][scan[0][i]];
	else
	    for (i = 2; i >= ncolors; i--)
		bcopy(scan[0], scan[i], nrow);
	map_scanline(scan, nrow, 1, y,
		     &buffer[(hdr.ymax - y) * nbyte]);
	/* Subsample image to create icon */
	if ( (hdr.ymax - y) % iconfact == 0 )
	    map_scanline( scan, iconrow, iconfact,
			  (hdr.ymax - y) / iconfact,
			  &iconbuf[((hdr.ymax - y) / iconfact) *
				   iconbyte] );
    	if ( setbg < 2 )
	    put_scanline(&buffer[(hdr.ymax - y) * nbyte], nrow, 0,
		     hdr.ymax - y );
    }
    /*
     * Free temp storage 
     */
    for (i = 0; i < 3; i++)
	free(scan[i]);

    hdr.xmin = xmin_original;
    hdr.xmax += xmin_original;
}
/* 
 * Track events & redraw image when necessary.
 */
update_pic( use_pix )
{
    int             i,
                    npix,
                    pixscan,
                    lastscan,
                    gotpix;
    long            bufsize,
                    pixsize;
    XEvent          rep;
    XButtonEvent xkey;
    XExposeEvent    xex;
    int zoom_x_press, zoom_y_press;

    Pixmap(*pix)[] = 0;
    Bitmap bm;

    if (zoomflg)
    {
	zoom_x_center = nrow / 2;
	zoom_y_center = nscan / 2;
	update_zoom();
    }

    /*
     * If requested, use off screen pixmap to speed image redisplay. Need to
     * allocate it in slices, since can't write whole image to X at once. 
     */
    if (use_pix)
    {
	npix = 1 + (nscan * nbyte) / MAXSEND;
	pixscan = MAXSEND / nbyte;
	pixsize = pixscan * nbyte;
	lastscan = nscan % pixscan;

	pix = (Pixmap(*)[]) malloc(npix * sizeof(Pixmap));
	for (i = 0; i < npix; i++)
	    if ( bwflag != 2 )
		(*pix)[i] = XStorePixmapZ(nrow,
					  (i == npix - 1) ? lastscan : pixscan,
					  &buffer[i * nrow * pixscan]);
	    else
	    {
		 bm = XStoreBitmap(nrow,
				   (i == npix - 1) ? lastscan : pixscan,
				   &buffer[i * nbyte * pixscan]);
		 (*pix)[i] = XMakePixmap( bm, 1, 0 );
		 XFreeBitmap( bm );
	     }
    }

    if ( setbg )
    {
	XChangeBackground( RootWindow, (*pix)[0] );
	XClear( RootWindow );
	XFlush();
    	if ( setbg == 2 )
    	    exit( 0 );
    }

    /*
     * Basic event loop:  handle expose events on window & icon, and exit on
     * (shifted) mouse button event. 
     */
    for (;;)
    {
	XNextEvent(&rep);
	if (rep.type == ButtonPressed)
	{
	    xkey = *(XButtonEvent *) &rep;
	    if (zoomflg && (xkey.window == fbwin))
	    {
		zoom_x_press = xkey.x;
		zoom_y_press = xkey.y;
	    }
	}
	else if (rep.type == ButtonReleased)
	{
	    xkey = *(XButtonEvent *) &rep;
	    if ( xkey.detail & ShiftMask )
	    {
		if (zoomflg && (xkey.window == zoomwin))
		{
		    /* get rid of just zoom window if shiftclicked */
		    XDestroyWindow(zoomwin);
		    zoomflg = 0;
		}
		else
		{
		    /* Shiftclick in fbwin means exit. */
		    break;
		}
	    }
	    if (zoomflg && (xkey.window == fbwin))
	    {
		zoom_x_center = xkey.x;
		zoom_y_center = xkey.y;
		if ((zoom_x_center != zoom_x_press) &&
		    (zoom_y_center != zoom_y_press))
		{
		    /* drag to define zoom region */
		    if (zoom_x_press < zoom_x_center)
		    {
			zoom_x_dim = (zoom_x_center - zoom_x_press);
			zoom_x_center = zoom_x_press + (zoom_x_dim / 2);
		    }
		    else
		    {
			zoom_x_dim = (zoom_x_press - zoom_x_center);
			zoom_x_center = zoom_x_center + (zoom_x_dim / 2);
		    }
		    if (zoom_y_press < zoom_y_center)
		    {
			zoom_y_dim = (zoom_y_center - zoom_y_press);
			zoom_y_center = zoom_y_press + (zoom_y_dim / 2);
		    }
		    else
		    {
			zoom_y_dim = (zoom_y_press - zoom_y_center);
			zoom_y_center = zoom_y_center + (zoom_y_dim / 2);
		    }
		    XChangeWindow(zoomwin, zoomfact * zoom_x_dim, 
				  zoomfact * zoom_y_dim);
		    XSetResizeHint( zoomwin, 0, 0, zoomfact, zoomfact );
		}
		update_zoom();
	    }
	    else if (zoomflg && (xkey.window == zoomwin))
	    {
		if (xkey.detail & RightMask)
		{
		    /* increase zoom factor */
		    zoomfact += 1;
		    XChangeWindow(zoomwin, zoomfact * zoom_x_dim, 
				  zoomfact * zoom_y_dim);
		    XSetResizeHint( zoomwin, 0, 0, zoomfact, zoomfact );
		}
		else if (xkey.detail & LeftMask)
		{
		    /* decrease zoom factor */
		    if (zoomfact > 1)
		    {
			zoomfact -= 1;
			XChangeWindow(zoomwin, zoomfact * zoom_x_dim, 
				      zoomfact * zoom_y_dim);
			XSetResizeHint( zoomwin, 0, 0, zoomfact, zoomfact );
		    }
		}
		else if (xkey.detail & MiddleMask)
		{
		    if (forkflg)
		    {
			/* can only report pixel status when not forking */
			fprintf(stderr, "Position: (%d, %d)\n",
				hdr.xmin +
				xkey.x / zoomfact + (zoom_x_center 
						     - (zoom_x_dim / 2)), 
				(hdr.ymax 
				 - xkey.y / zoomfact 
				 - (zoom_y_center - (zoom_y_dim / 2))));
			fprintf(stderr, "Image crop: (%d, %d) to (%d, %d)\n",
				hdr.xmin, hdr.ymin,
				hdr.xmax, hdr.ymax);
			fprintf(stderr, "Zoom crop: (%d, %d) to (%d, %d)\n",
				hdr.xmin + 
				(zoom_x_center - (zoom_x_dim / 2)), 
				(hdr.ymax - (zoom_y_dim - 1) 
				 - (zoom_y_center - (zoom_y_dim / 2))),
				hdr.xmin + (zoom_x_dim - 1)
				+ (zoom_x_center - (zoom_x_dim / 2)),
				(hdr.ymax
				 - (zoom_y_center - (zoom_y_dim / 2))));


		    }
		}
	    }
	}
	else if (rep.type == ExposeWindow || rep.type == ExposeRegion)
	{
	    xex = *((XExposeEvent *) & rep);
	    /*
	     * For icon exposure, just redraw whole thing - it's quick and
	     * much easier. 
	     */
	    if (xex.window == iconwin)
		if ( bwflag != 2 )
		    XPixmapBitsPutZ(iconwin, 0, 0, iconrow, iconscan,
				    iconbuf, 0, GXcopy, AllPlanes);
		else
		    XBitmapBitsPut(iconwin, 0, 0, iconrow, iconscan,
				   iconbuf, 1, 0, 0, GXcopy, AllPlanes);
	    else if (zoomflg && (xex.window == zoomwin))
	    {
		resize_zoom_window();
		update_zoom();
	    }
	    else
	    {
		/*
		 * If window has been resized (bigger), don't bother redrawing
		 * the area outside the image. 
		 */
		if (xex.y + xex.height >= nscan)
		    xex.height = nscan - xex.y;
		/*
		 * If bitmap, round beginning pixel to beginning of word
		 */
		if ( bwflag == 2 )
		{
		    xex.width += xex.x;	/* remember ending pixel */
		    xex.x = (xex.x / 16) * 16;	/* round down */
		    xex.width -= xex.x;	/* new width */
		}
		if (xex.x + xex.width >= nrow)
		    xex.width = nrow - xex.x;
		/*
		 * If no pixmap, do it the slow way. 
		 */
		if (pix == 0)
		    if ( bwflag == 2 )
			for (i = xex.y; i < xex.y + xex.height; i++)
			    put_scanline( &buffer[i * nbyte + xex.x / 8],
					  xex.width, xex.x, i );
		    else
			for (i = xex.y; i < xex.y + xex.height; i++)
			    put_scanline( &buffer[i * nbyte + xex.x],
					  xex.width, xex.x, i );
		else
		{
		    /*
		     * Pixmaps exist, figure out how many slices are affected
		     * and which portion of each. 
		     */
		    int             start = xex.y / pixscan,
		                    end = (xex.y + xex.height) / pixscan,
		                    y = xex.y,
		                    ytop;
		    register int    j;

		    for (i = start; i <= end; i++)
		    {
			ytop = (i == end) ? xex.y + xex.height
			    : (i + 1) * pixscan;
			/*
			 * if this slice has a pixmap, blit it.  If not, do it
			 * slow from the buffer. 
			 */
			if ((*pix)[i] != 0)
			    XPixmapPut(fbwin, xex.x, y - i * pixscan,
				       xex.x, y,
				       xex.width, ytop - y, (*pix)[i],
				       GXcopy, AllPlanes);
			else
			    if ( bwflag == 2 )
				for (j = y; j < ytop; j++)
				    put_scanline( &buffer[j * nbyte +
							  xex.x / 8],
						  xex.width, xex.x, j );
			    else
				for (j = y; j < ytop; j++)
				    put_scanline( &buffer[j * nbyte + xex.x],
						  xex.width, xex.x, j );
			y = ytop;
		    }
		}
	    }
	}
	else if ( rep.type != ButtonPressed )
	    fprintf(stderr, "%s: Event type %x?\n", hdr.cmd, rep.type);
    }

    /*
     * Window goes away when we do. 
     */
    XDestroyWindow(fbwin);
    if (zoomflg) XDestroyWindow(zoomwin);
}

update_zoom()
{
    unsigned char the_pix;
    int i,j;
    int zoomx, zoomy;
    int x_bottom, x_top;
    int y_bottom, y_top;
    int zoom_x_corner, zoom_y_corner;

    x_bottom = zoom_x_center - (zoom_x_dim / 2);
    x_top = x_bottom + zoom_x_dim - 1;
    y_bottom = zoom_y_center - (zoom_y_dim / 2);
    y_top = y_bottom + zoom_y_dim - 1;

    for (j=y_bottom; j<=y_top; j++)
    {
	zoom_y_corner = (j - y_bottom) * zoomfact;
	if ((j < 0) || (j >= nscan))
	{
	    /* dump a blank line */
	    XPixSet( zoomwin, 0, zoom_y_corner, zoomfact * zoom_x_dim, 
		    zoomfact, colmap[0].pixel );
	}
	else
	{
	    for ( i= x_bottom; i<=x_top; i++)
	    {
		zoom_x_corner = (i - x_bottom) * zoomfact;
		if ((i < 0) || (i >= nrow))
		{
		    /* dump a black pixel */
		    XPixSet( zoomwin, zoom_x_corner, zoom_y_corner,
			    zoomfact, zoomfact, colmap[0].pixel );
		}
		else
		{
		    if ( bwflag != 2 )
			the_pix = buffer[j * nbyte + i];
		    else
			the_pix = buffer[j * nbyte + i/8] & (1 << (i % 8)) ?
			    1 : 0;
		    XPixSet( zoomwin, zoom_x_corner, zoom_y_corner,
			    zoomfact, zoomfact, the_pix );
		}
	    }
	}
    }
}

/* 
 * Create a window with help from user.
 */
create_window( width, height, window_geometry )
{
    OpaqueFrame frame;
    char geometry[30];
    int zoomwidth, zoomheight;

    /* 
     * Now, make the window.
     */
    sprintf( geometry, "=%dx%d+0+0", nrow, nscan );
    frame.bdrwidth = 3;
    frame.border = WhitePixmap;
    frame.background = BlackPixmap;
    if ( (fbwin = XCreate( hdr.cmd, hdr.cmd, window_geometry, geometry,
			   &frame, nrow, nscan )) == NULL )
    {
	fprintf( stderr, "%s: Window Create failed\n", hdr.cmd );
	exit( 1 );
    }
    XMapWindow( fbwin );
    XGetHardwareColor( &colmap[0] );	/* Assume black! */
    XPixSet( fbwin, 0, 0, width, height, colmap[0].pixel );
    XSetResizeHint( fbwin, width, height, 1, 1 );
    XSelectInput( fbwin, ButtonPressed|ButtonReleased|ExposeRegion );

    if (zoomflg)
    {
	arrow_curs = XCreateCursor(arrow_width,arrow_height,arrow_bits,
			     arrow_mask_bits, arrow_x_hot, arrow_y_hot,
			     BlackPixel, WhitePixel, GXcopy);
	XDefineCursor(fbwin,arrow_curs);
    }

    iconwin = XCreateWindow( RootWindow, 1, 1, 
			     width / iconfact, height / iconfact, 0, 0, 0 );
    XTileRelative( iconwin );
    XSetIconWindow( fbwin, iconwin );
    XSelectInput( iconwin, ButtonPressed|ButtonReleased|ExposeWindow );
    
    if (zoomflg)
    {
	zoomwidth = zoomfact * zoom_x_dim;
	zoomheight = zoomfact * zoom_y_dim;
	sprintf( geometry, "=%dx%d+0+0", zoomwidth, zoomheight );
	frame.bdrwidth = 3;
	frame.border = WhitePixmap;
	frame.background = BlackPixmap;
	if ( (zoomwin = XCreate( "getx10 zoom", hdr.cmd, window_geometry, 
				geometry,
				&frame, zoomwidth, zoomheight )) == NULL )
	{
	    fprintf( stderr, "%s: Zoom window Create failed\n", hdr.cmd );
	    exit( 1 );
	}
	resize_zoom_window();
	XMapWindow( zoomwin );
	XPixSet( zoomwin, 0, 0, zoomfact * zoom_x_dim, zoomfact * zoom_y_dim,
		colmap[0].pixel );
	XSetResizeHint( zoomwin, 0, 0, zoomfact, zoomfact );
	XSelectInput( zoomwin, ButtonPressed|ButtonReleased|ExposeRegion );
	XDefineCursor(zoomwin,arrow_curs);
    }
}

/*
 * Map a scanline to 8 bits through the dither matrix.
 * 
 * Inputs:
 * 	rgb:		Pointers to buffers containing the red, green,
 *			and blue color rows.
 *	n:		Length of row.
 *	s:		Skip between pixels in original image.
 * 	y:		Y position of row (necessary for dither)
 *	line:		Pointer to output buffer for dithered color data.
 */
#define DMAP(v,x,y)	(modN[v]>dm16[x][y] ? divN[v] + 1 : divN[v])

map_scanline( rgb, n, s, y, line )
unsigned char *rgb[3], *line;
{
    register int i, col, row;

    if ( usemap )
    {
	register unsigned char *r;

	for ( r = rgb[0], i = 0; i < n; i++, r += s )
	    line[i] = colmap[*r].pixel;
    }
    else if ( !bwflag )
    {
	register unsigned char *r, *g, *b;
	for ( row = y % 16, col = 0, i = 0, r = rgb[0], g = rgb[1], b = rgb[2];
	      i < n; i++, r+=s, g+=s, b+=s, col = ((col + 1) & 15) )
	    line[i] = colmap[DMAP(in_cmap[0][*r], col, row) +
			     DMAP(in_cmap[1][*g], col, row) * levels +
			     DMAP(in_cmap[2][*b], col, row) * levelsq].pixel;
    }
    else if ( bwflag == 1 )	/* gray scale display */
    {
	register unsigned char *r;

	for ( row = y % 16, col = 0, i = 0, r = rgb[0];
	      i < n; i++, r+=s, col = ((col + 1) & 15) )
	    line[i] = colmap[DMAP(*r, col, row)].pixel;
    }
    else			/* bitmap display */
    {
	register unsigned short *l = (unsigned short *)line;
	register unsigned char * r;

	for ( row = y % 16, col = 0, i = 0, r = rgb[0], *l = 0;
	      i < n; r+=s, col = ((col + 1) & 15) )
	{
	    *l |= (*r > dm16[col][row] ? 1 : 0) << (i % 16);
	    if ( (++i % 16) == 0 && i < n )
		*++l = 0;
	}
    }
}

put_scanline( scan, width, x, y )
unsigned char *scan;
{
    if ( bwflag != 2 )
	XPixmapBitsPutZ( fbwin, x, y, width, 1, scan, 0, GXcopy, AllPlanes );
    else
	XBitmapBitsPut( fbwin, x, y, width, 1, scan, 1, 0, 0, GXcopy,
			AllPlanes );
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
map_rgb_to_bw( red_row, green_row, blue_row, bw_row, map, rowlen )
rle_pixel *red_row;
rle_pixel *green_row;
rle_pixel *blue_row;
rle_pixel *bw_row;
rle_pixel **map;
{
    register int x, bw;

    for (x=0; x<rowlen; x++)
    {
	/* 68000 won't store float > 127 into byte? */
	/* HP compiler blows it */
	bw = .35*map[0][red_row[x]] + .55*map[1][green_row[x]] +
	    .10*map[2][blue_row[x]];
	bw_row[x] = bw;
    }
}


/*
 * This routine builds the color map (for X window system, details may
 * differ in your system.  Note particularly the two level color
 * mapping:  X will give back 216 color map entries, but there is no
 * guarantee they are sequential.  The dithering code above wants to
 * compute a number in 0 - 215, so we must map from this to the X color
 * map entry id.
 */
/*
 * Initialize the 8 bit color map.  Use gamma corrected map.
 */

init_color()
{
    int i, rgbmap[256][3], bwmap[256];
#ifdef DEBUG
    int j;
#endif

    if ( !bwflag )
    {
	/* 
	 * Figure out how many color map entries we can get
	 */
	if ( levels == 0 )
	    levels = 6;		/* default starting point */
	for ( ; levels >= 2; levels-- )
	    if ( levels*levels*levels < DisplayCells() )
		break;
	if ( levels < 2 )
	{
	    fprintf(stderr,
		    "%s: This display doesn't have enough color resolution.\
\nWill produce black and white image instead.\n", hdr.cmd );
	    bwflag = 1;
	    levels = 0;
	}
	/*
	 * levels is the maximum number we can get.  Now we have to try to
	 * actually acquire the color map entries.
	 */
	while ( levels >= 2 )
	{
	    levelsq = levels * levels;
	    levelsc = levelsq * levels;

	    dithermap( levels, disp_gam, rgbmap, divN, modN, dm16 );

	    /* 
	     * Set up the color map entries.  We don't yet know the location
	     * in the map at which each will reside, so init it to 0.
	     */
	    for(i = 0; i < levelsc; i++) {
		colmap[i].pixel = 0;
		colmap[i].red = rgbmap[i][0] << 8;
		colmap[i].green = rgbmap[i][1] << 8;
		colmap[i].blue = rgbmap[i][2] << 8;
	    }

	    /* Get a color map entry for each color.  Assume enough exist! */
	    for ( i = 0; i < levelsc; i++ )
	    {
		if ( XGetHardwareColor( &colmap[i] ) == 0 )
		    break;
	    }

	    /* Check if the colors are available */
	    if ( i < levelsc )
	    {
		levels--;
		/* Free the colors already obtained */
		for ( i--; i >= 0; i-- )
		    XFreeColors( &colmap[i].pixel, 1, 0 );
		continue;
	    }
	    break;		/* succeeded */
	}
	if ( levels < 2 && bwflag == 0 )
	{
	    fprintf( stderr,
		     "%s: Sorry, not enough color map entries are available.\
\nWill make black & white image instead.\n", hdr.cmd );
	    bwflag = 1;
	    levels = 0;
	}
    }

    /* Get a B&W color map */
    if ( bwflag == 1 )
    {
	/* Try for lots of levels, give up by factors of 2 until success */
	if ( levels == 0 )
	    if ( DisplayCells() > 16 )
		levels = DisplayCells() / 2;
	    else
		levels = DisplayCells() - 4;	/* assume this many taken */
	for ( ; levels > 1; (levels > 16) ? (levels /= 2) : (levels -= 1) )
	{
	    bwdithermap( levels, disp_gam, bwmap, divN, modN, dm16 );

	    /* 
	     * Set up the color map entries.  We don't yet know the location
	     * in the map at which each will reside, so init it to 0.
	     */
	    for ( i = 0; i < levels; i++ )
	    {
		colmap[i].pixel = 0;
		colmap[i].red = bwmap[i] << 8;
		colmap[i].green = colmap[i].red;
		colmap[i].blue = colmap[i].red;
	    }

	    /* Get a color map entry for each color.  Assume enough exist! */
	    for ( i = 0; i < levels; i++ )
	    {
		if ( XGetHardwareColor( &colmap[i] ) == 0 )
		    break;
	    }

	    /* Check if the colors are available */
	    if ( i < levels )
	    {
		/* Free the colors already obtained */
		for ( i--; i >= 0; i-- )
		    XFreeColors( &colmap[i].pixel, 1, 0 );
		continue;
	    }
	    break;		/* succeeded */
	}
	if ( levels < 2 )
	    bwflag = 2;		/* use 1 bit dithering */
	else
	    levelsc = levels;	/* remember full range */
    }

    /* If b&w 1-bit display, just use two colors */
    if ( bwflag == 2 )
    {
	/* All we care about, really, is the magic square */
	make_square( 255.0, divN, modN, dm16 );
	levels = levelsc = 2;
    }

#ifdef DEBUG
    printf( "Magic square for %d levels:\n", levels );
    for ( i = 0; i < 16; i++ )
    {
	for ( j = 0; j < 16; j++ )
	    printf( "%4d", dm16[i][j] );
	printf( "\n" );
    }
    printf( "divN array:\n" );
    for ( i = 0; i < 256; i++ )
	printf( "%4d%s", divN[i], i % 16 == 15 ? "\n" : "" );
    printf( "modN array:\n" );
    for ( i = 0; i < 256; i++ )
	printf( "%4d%s", modN[i], i % 16 == 15 ? "\n" : "" );
#endif
}

/*****************************************************************
 * TAG( load_x_map )
 * 
 * Loads the color map from the input RLE file directly into X window
 * system.
 * Inputs:
 * 	in_cmap:	Global variable containing gamma corrected color
 * 			map from input file.
 * Outputs:
 * 	Loads color map (further gamma corrected for display) into
 * 	display.  If not enough display map entries are available,
 * 	prints a message and exits the program.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
load_x_map()
{
    int nmap = 0, i, j;
    char * mapcom;
    int gammamap[256];
    
    if ( (mapcom = rle_getcom( "color_map_length", &hdr )) )
	nmap = atoi( mapcom );
    if ( nmap == 0 )
	nmap = 1 << hdr.cmaplen;

    /* We won't be using in_cmap, so gamma correct it in place */
    for ( i = 0; i < 256; i++ )
	gammamap[i] = (int)(0.5 + 255 * pow( i / 255.0, 1.0/disp_gam ));

    for ( i = 0; i < nmap; i++ )
	for ( j = 0; j < 3; j++ )
	    in_cmap[j][i] = gammamap[in_cmap[j][i]];

    /* 
     * Set up the color map entries.  We don't yet know the location
     * in the map at which each will reside, so init it to 0.
     */
    for(i = 0; i < nmap; i++) {
	colmap[i].pixel = 0;
	colmap[i].red = in_cmap[0][i] << 8;
	colmap[i].green = in_cmap[1][i] << 8;
	colmap[i].blue = in_cmap[2][i] << 8;
	if ( XGetHardwareColor( &colmap[i] ) == 0 )
	{
	    fprintf( stderr,
	    "%s: Sorry, need at least %d color map entries, only got %d.\n\
Will display image anyway.\n",
		     hdr.cmd, nmap, i );
	    break;
	}
    }
    for ( ; i < 256; i++ )
	colmap[i].pixel = 0;	/* for "undefined" colors */
}

resize_zoom_window()
{
    WindowInfo info;
    int xs, ys;

    XQueryWindow( zoomwin, &info );

    /* truncate to multiple of zoomfact */
    xs = zoomfact * ( info.width / zoomfact );
    ys = zoomfact * ( info.height / zoomfact );

    if ( xs != info.width || ys != info.height )
	XChangeWindow( zoomwin, xs, ys );

    zoom_x_dim = xs / zoomfact;
    zoom_y_dim = ys / zoomfact;
}
