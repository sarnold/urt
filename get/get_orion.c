/*
 * getOrion.c - Put RLE images on a High-Level Hardware Orion
 * 
 * Author:	Gianpaolo F M Tommasi
 *		Computer Laboratory
 *		University of Cambridge
 * Last Edit:   19 July 1987
 *
 * Based on code by Spencer W Thomas, John W Peterson and Mark Blommenthal
 * Copyright (c) 1986, University of Utah
 *
 */

#include <stdio.h>
#include <math.h>
#include <graphics/grafix.h>
#include <graphics/window.h>
#include <graphics/control.h>
#include <graphics/events.h>
#include <sys/ioctl.h>
#include <machine/graphics.h>
#include "rle.h"

#define RASTERSIZE_LIM		1280
#define MONOCHROME_MODE		1
#define EIGHT_BIT_COLOUR_MODE	2
#define MONOCHROME_ENTRIES	128

/* TRUE and FALSE defined in graphics/grafix.h */

typedef int boolean;		/* logical vars and values */

/* RLE image header. */
rle_hdr *hdr;

/* RLE Scanline storage & row pointers */
unsigned char scanline[4][RASTERSIZE_LIM], *rows[4];

/* Scanline to be written to device points to scanline bitmap*/
unsigned char dest_pixels[RASTERSIZE_LIM];

/* Variables for translate_bits: bitmap for scan line and rectangle */
BitMap scanbitmap;
Rect trandr;


/* dither matrix */
int dm16[16][16];

/* tables for quantisation */
int errN[256], divN[256];

/* The gamma map and default gamma */
int gammamap[256];
float gam = 2.0;	/* good default value */

/* Colour map flags */
boolean linear_flag = FALSE;
boolean gamma_flag = FALSE;
boolean all_colours_flag = FALSE;

/* What mode to dither into.  Either 8 bit color or monochrome */
int put_mode = EIGHT_BIT_COLOUR_MODE;

/* Colour map to be written to device */
int n_colmap_colours;
int colmap[256][3];

/* StarPoint variables */
WindowPtr rle_window;
EventRecord myEvent;
WindowPtr whichWindow;
Point myPt;
int thePart;
boolean isActive = FALSE;
int window_size_x, window_size_y;

/* Debug flag */
boolean debug_flag = FALSE;

/* reverse order of scan lines */
boolean reverse_flag = FALSE;

/* make a special case of having 1 colour channel and 3 colour map
   channels */
boolean onec_threem_mode = FALSE;


/****************************************************************************
 * TAG( main )
 * 
 * Usage:
 *	getOrion [-D] [-b] [-f] [-g gamma] [-l] [-r] [file]
 *
 * Inputs:
 *	-D:	    Degub mode.
 *	-b:	    Dither to monochrome picture
 *	-f:	    Use all the 256 entries of the device colour look-up table
 *	-g gam:	    Gamma map, use a gamma of gam (floating point number)
 *	-l:	    Linear map (gamma of 1)
 *	-r:	    Reverse the order of display of scanlines, i.e. start from
 
 *			the top
 *    file:	    Input Run Length Encoded file, stdin used if not
 *			specified.
 * Outputs:
 *	Puts image on the screen.
 * Assumptions:
 *	Input file is in RLE format.
 * Algorithm:
 *	[none]
 */

main(argc, argv)
int argc;
char *argv[];
{
    int i, temp;
    int bw_flag = 0;
    char *infname = NULL, *window_name;
    FILE *fopen();
    Rect r;

    hdr = *rle_hdr_init( (rle_hdr *)NULL );

    /* Get command line arguments */
    if (scanargs(argc, argv, "getOrion D%- w%- f%- g%-gamma!f l%- r%- infile
%s",
	 &debug_flag, &bw_flag, &all_colours_flag, 
	 &gamma_flag, &gam, &linear_flag, &reverse_flag, &infname) == 0)
	exit(1);
    
    /* Open file for reading, if no filename was specified, read from
       stdin */
    rle_names( &hdr, cmd_name( argv ), infname, 0 );
    hdr.rle_file = rle_open_f(hdr.cmd, infname, "r");

    temp = rle_get_setup(&hdr);
    if (temp < 0) {
	fprintf(stderr, "getOrion: error reading setup information from %s\n",
		infname ? infname : "stdin");
	fprintf(stderr, "with return code %d\n", temp);
	exit(1);
	}

    if (debug_flag) rle_debug(1);

    /* a linear map has no gamma correction */
    if (linear_flag) gam = 1.0;

    /* Set up gamma correction table */
    init_gamma_map();

    /* we're only interested in R, G and B */
    RLE_CLR_BIT(hdr, RLE_ALPHA);
    for (i = 3; i < hdr.ncolors; i++)
	RLE_CLR_BIT(hdr, i);
    
    /* Set up rows to point to our copy of the scanline */
    for (i = 0; i < 3; i++)
	rows[i] = scanline[i];

    /* Pretend origin is at 0,0 */
    hdr.xmax -= hdr.xmin;
    hdr.xmin = 0;
    hdr.ymax -= hdr.ymin;
    hdr.ymin = 0;

    /* shift the colour map, if present, down to 8 bits precision */
    for (i = 0; i < hdr.ncmap * (1 << hdr.cmaplen); i++)
	hdr.cmap[i] = hdr.cmap[i] >> 8;

    if (bw_flag) {
	put_mode = MONOCHROME_MODE;
	init_monochrome_colour_map();
	}
    else if ((hdr.ncmap == 3) && hdr.ncolors == 1) {
        /* one channel and 24-bit colour map supplied */
	init_24_bit_colour_map();
	onec_threem_mode = TRUE;
	}
    else
	/* use 216 standard colour map */
	init_8_bit_colour_map();

    /* Initialise the device and create window */
    InitGraf();
    window_size_x = hdr.xmax + 1;
    window_size_y = hdr.ymax + 1;
    SetRect (&r, 50, 50, window_size_x, window_size_y);
    if (infname == NULL)
	window_name = "piped";
    else {
	window_name = infname;
    }
    rle_window = NewWindow(NIL, r, window_name, VISIBLE, noGrowDocProc, FRONT,
			 GOAWAY, 0, 7);
    PenPat(black);
    UpdateWindow(rle_window, TRUE);
    write_colour_map();

    /* make the scan line bit map */
    scanbitmap = NewRBitMap(window_size_x, 1, 8);  /* really need only one pla
ne */

    /* Get the scanlines */
    for (i = hdr.ymin; i <= hdr.ymax; i++) {
	rle_getrow(&hdr, rows);
	if (put_mode == MONOCHROME_MODE)
	    put_line_mono(i);
	else
	    put_line_8(i);
    }

    /* loop */
    EventLoop();
}




/*************************************************
 * TAG (init_gamma_map )
 *
 * Compute a gamma correction map
 */

init_gamma_map()
{
    int i;

    for (i = 0; i < 256; i++) {
	    gammamap[i] = (int) (0.5 + 255 * pow(i / 255.0, 1.0/gam));
	}
}


/*********************************
 * TAG( init_8_bit_colour_map )
 * 
 * Initialise the 8 bit colour map
 */

init_8_bit_colour_map()
{
    double gamma;

    /* Set up the colour map entries. We will use 216 colours. */
    n_colmap_colours = 216;

    /* calculate tables but don't gamma correct the colour map */
    gamma = 1.0;
    dithermap(6, gamma, colmap, divN, errN, dm16);
}


/************************************************************
 * TAG( init_monochrome_colour_map )
 *
 * Initialize the monchrome colour map.  This routine is called when the -b 
 * flag is used.
 */

init_monochrome_colour_map()
{
    int i, bwmap[256];
    double gamma;

    /* Set up the colour map entries into a single greyscale ramp */
    n_colmap_colours = MONOCHROME_ENTRIES;

    /* calculate tables but don't gamma correct the colour map */
    gamma = 1.0;
    bwdithermap(n_colmap_colours, gamma, bwmap, divN, errN, dm16);
    for (i = 0; i < n_colmap_colours; i++)
	colmap[i][0] = colmap[i][1] = colmap[i][2] = bwmap[i];

}



/***********************************************
 * TAG (init_24_bit_colour_map)
 *
 * Sets the colour map to the one supplied in the RLE header
 *
 */

init_24_bit_colour_map()
{
    int i, g_offset, b_offset;

    /* set up the colour map entries, will use the number supplied in the
       header */
    n_colmap_colours = 1 << hdr.cmaplen;
    g_offset = n_colmap_colours;
    b_offset = 2 * g_offset;

    /* set the device colour map to the one in the header  */
    for (i = 0; i < n_colmap_colours; i++) {
	colmap[i][0] = hdr.cmap[i];
	colmap[i][1] = hdr.cmap[i + g_offset];
	colmap[i][2] = hdr.cmap[i + b_offset];
    }
}



/* Dithering operation */
#define DMAP(v, x, y)	    (errN[v]>dm16[x][y] ? divN[v]+1 : divN[v])

/************************************************************
 * TAG(put_line_8)
 *
 * Map a 24 bit scanline to 8 bits through the dither matrix 
 */

put_line_8(yscan)
int yscan;
{
    unsigned char *r, *g, *b;
    int i, dither_col, dither_row;
    int xmax = hdr.xmax;
    int g_offset, b_offset;
    unsigned char *dest_pixel_ptr;

    /* treat 1 colour and 3 colmap channels as special case */
    if (onec_threem_mode) {
	bcopy(rows[0], dest_pixels, xmax+1);
	write_scanline(yscan);
	return;
    }

    dither_row = yscan % 16;
    dither_col = 0;
    dest_pixel_ptr = dest_pixels;
    r = rows[0];
    /* check if we have less than three colour channels */
    g = rows[(hdr.ncolors >= 2 ? 1 : 0)];
    b = rows[(hdr.ncolors >= 3 ? 2 : 0)];

    /* RLE file with colour map */
    if (hdr.ncmap) {
	/* offsets to the green and blue sections of the map */
	g_offset = 1 << hdr.cmaplen;
	b_offset = 2 * g_offset;

	for (i = 0; i <= xmax; i++, r++, g++, b++,
			      dither_col = ((dither_col + 1) & 15),
			      dest_pixel_ptr++)
	    *dest_pixel_ptr =
	      DMAP(hdr.cmap[*r], dither_col, dither_row) +
	      DMAP(hdr.cmap[*g + g_offset], dither_col,
					dither_row) * 6 +
	      DMAP(hdr.cmap[*b + b_offset], dither_col,
					dither_row) * 36;
	}
    
    else 
	for (i = 0; i <= xmax; i++, r++, g++, b++,
			      dither_col = ((dither_col + 1) & 15),
			      dest_pixel_ptr++) {
	    *dest_pixel_ptr =
	      DMAP(*r, dither_col, dither_row) +
	      DMAP(*g, dither_col, dither_row) * 6 +
	      DMAP(*b, dither_col, dither_row) * 36;
	    }

    /* Write the scanline to the device */
    write_scanline(yscan);
}


/*****************************************************************
 * TAG( put_line_mono )
 *
 * Dither into monochrome
 */

put_line_mono(yscan)
int yscan;
{
    unsigned char *r, *g, *b;
    int i, dither_col, dither_row;
    unsigned char *dest_pixel_ptr;
    int xmax = hdr.xmax;
    int bw_val;
    int g_offset, b_offset;

    dither_row = yscan % 16;
    dither_col = 0;
    dest_pixel_ptr = dest_pixels;

    r = rows[0];
    /* check if we have less than three colour channels */
    g = rows[(hdr.ncolors >= 2 ? 1 : 0)];
    b = rows[(hdr.ncolors >= 3 ? 2 : 0)];

    /* RLE file with colour map */
    if (hdr.ncmap) {
	/* offsets in table */
	g_offset = 1 << hdr.cmaplen;
	b_offset = 2 * g_offset;

	for (i = 0; i <= xmax; i++, r++, g++, b++,
			      dither_col = ((dither_col + 1) & 15),
			      dest_pixel_ptr++) {
	    bw_val =
		(35 * hdr.cmap[*r] +
		 55 * hdr.cmap[*g + g_offset] +
		 10 * hdr.cmap[*b + b_offset]) / 100;
	    *dest_pixel_ptr = DMAP(bw_val, dither_col, dither_row);
	    }
	}

    /* Gamma correction is the default */
    else 
	for (i = 0; i <= xmax; i++, r++, g++, b++,
			      dither_col = ((dither_col + 1) & 15),
			      dest_pixel_ptr++) {
	    bw_val =
		(35*(*r) + 55*(*g) + 10*(*b)) / 100;
	    *dest_pixel_ptr =
		DMAP(bw_val, dither_col, dither_row);
	    }

    /* write the scanline */
    write_scanline(yscan);
}


/* graphics device colour look-up table operations */
#define WriteCLT(cltp)  (ioctl (_gdev, GTIOCSCLT, &(cltp)))
#define ReadCLT(cltp)   (ioctl (_gdev, GTIOCGCLT, &(cltp)))

/***********************************************
 * TAG ( write_colour_map )
 *
 * Loads the graphics colour look up table 
 */

write_colour_map()
{
    int cltable[256], i;
    int *cltp = cltable;  /* to avoid warning */
    int terminate;
    
    /* Need to preserve colours 240-255 by default */
    if (all_colours_flag)
	terminate = 256;
    else {
	/* read in current CLT */
	ReadCLT(cltp);
	terminate = (n_colmap_colours < 240 ? n_colmap_colours : 240);
	}

    /* create the colour look up table, changing only required number of
       colours, and gamma correcting  */
    for (i = 0; i < terminate; i++)
	cltable[i] = (gammamap[colmap[i][0]] << 16) + 
	    (gammamap[colmap[i][1]] << 8) + 
	    gammamap[colmap[i][2]];
    
  /* Write it to the device */ 
   WriteCLT(cltp);
}


/********************************************************
 * TAG ( write_scanline )
 * 
 * writes the scanline data, held in dest_pixels, to the device 
 */
 
write_scanline(yscan)
int yscan;
{
    if (!reverse_flag)
	yscan = hdr.ymax - yscan;

    /* the following may seem like a long-winded way of doing things, 
     * but it's a lot quicker than setting points and lines with the usual 
     * routines   .
     */

    /* set destination rectangle */
    SetRect(&trandr, 0, yscan, window_size_x, 1);
 
   /* translate the scanline data to the corresponding strip in the
       window */
    translate_bits(&((*rle_window).port.portBits), trandr);

    /* add strip to the update list of window and update */
    AddUpdate(rle_window, trandr, 255);
    UpdateWindow(rle_window, TRUE);
}


/* translate_bits - takes a scanline (dest_pixels) and translates the bits 
 *   within the bytes so to be able to display it in BW mode.  It extracts
 *   a plane of bits made out of the bits from the same position within a 
 *   byte from the source scanline.  Each plane is then rasterop-ed to the 
 *   corresponding plane in the window bit map. The version is not general 
 *   since it has been tuned to the current requirement, a more general 
 *   version can be found in the StarPoint Library.
 * Inputs: db is the destination BitMap, 
 *         dr is a rectangle specifying where in the window to put the scanlin
e
 */

translate_bits(db, dr)
BitMapPtr db;
Rect dr;
{
    Ptr sa = scanbitmap.baseAddr;
    unsigned sw = scanbitmap.rowBits;
    Ptr da = db->baseAddr;
    int dy = top(dr);
    unsigned dw = db->rowBits;
    unsigned dh = db->colBits;
    unsigned dps = db->planeSize;
    int cw = width (db->bounds);
    int ch = height (db->bounds);
    unsigned w = window_size_x;
    int *scanad;
    unsigned char *srcad;
    register int dest;
    int bitpos, bytes_in_source;
    int plane = 7;	/* all planes are of interest */

    while(plane+1) {
	scanad = scanbitmap.baseAddr; /* baseAddr is always word-aligned */
	srcad = dest_pixels;
	bitpos = dest = 0;
	bytes_in_source = window_size_x;
	
	while(bytes_in_source){
	    /* *srcad++ >> plane is a logical shift */
	    dest = dest | (((*srcad++ >> plane) & 0x01) << bitpos);
	    if (bitpos++ == 31) {
		*scanad++ = dest;
		dest = bitpos = 0;
	    }
	    bytes_in_source--;
	}
	*scanad = dest; /* might be some residue */

	/* raster op the translated scanline to the plane in window */
	rasterop(sa, sw, 1, 0, 0,
	    da, dw, dh, 1, 23, cw, ch,
	    0, dy, w, 1, srcCopy);

	plane--;
	da += dps;
    }
}


/********************************************************************
 * TAG( EventLoop )
 *
 * Deals with StarPoint.
 */
 

EventLoop ()
{
    while (TRUE)
    {
	GetNextEvent (everyEvent, &myEvent);
	switch (myEvent.what)
	{
	case mouseDown:
	    whichWindow = (WindowPtr) myEvent.refCon;
	    switch (FindWindow (myEvent.where, whichWindow))
	    {
	    case inGoAway: 
		if (TrackGoAway (whichWindow, myEvent.where))
		{
		    MyClose ();
		}
	    break;

	    case inDrag: 
		DragWindow (whichWindow, myEvent.where, stdBoundsRect);
	    break;

	    case inContent: 
		if (whichWindow != FrontWindow ())
		{
		    BringToFront (whichWindow);
		}
		else
		{
		    myPt = myEvent.where;
		    GlobalToLocal (&myPt);
		}
	    break;
	    }
	break;	/* End of mouseDown */

	case activate:
	    MyActivate ();
	break;
	}	
    } /* while (TRUE) */
}


MyClose ()
{
    exit (0);
}



MyActivate ()
{
    WindowPtr theWindow;
    Bool theState;

    theWindow = (WindowPtr) myEvent.message;
    theState = (myEvent.modifiers & activateFlag) != 0;
    if (theState)
    {
	if (!isActive)
	{
	    HiliteWindow (theWindow, TRUE);
	    SetPort (theWindow);
	    isActive = TRUE;
	}
    }
    else
    {
	HiliteWindow (theWindow, FALSE);
	isActive = FALSE;
    }
}

