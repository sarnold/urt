/*
 * getsun - display an rle image on a sun workstation running the
 *          sun window environment.
 *
 * Bug reports, or better yet fixes/enhancements, may be sent to the author.
 * Author:
 *	Philip J. Klimbal
 *	RIACS
 *	MS 230-5
 *	NASA Ames Research Center
 *	Moffett Field, CA 94035
 *	..!ames!riacs!klimbal
 *	klimbal@riacs.edu
 *
 * Copyright (c) 1987 Research Institute for Advanced Computer Science 
 * All rights reserved. No explicit or implicit warrenty provided.   
 *
 * Based on getX - Put RLE images on X display,
 * by Spencer W. Thomas, University of Utah.
 * Copyright (c) 1986, University of Utah
 *
 *
 * Utah Copyright Header (getX):
 *
 * 	This software is copyrighted as noted below.  It may be freely copied,
 * 	modified, and redistributed, provided that the copyright notice is
 * 	preserved on all copies.
 *
 * 	There is no warranty or other guarantee of fitness for this software,
 * 	it is provided solely "as is".  Bug reports or fixes may be sent
 * 	to the author, who may or may not act on them as he desires.
 *
 * 	You may not include this software in a program or other software product
 * 	without supplying the source, or without informing the end-user that the
 * 	source is available for no extra charge.
 *
 *
 * cc .... -lrle -lsuntool -lsunwindow -lpixrect -lm
 */
static char rcsid[] = "$Header: /l/spencer/src/urt/get/RCS/getsun.c,v 3.0.1.2 1992/04/30 14:05:10 spencer Exp $";
/*
getsun()			Tag the file.
*/

#include <stdio.h>
#include <math.h>
#include <sys/file.h>
#include <suntool/sunview.h>
#include <suntool/canvas.h>
#include "rle.h"

void get_pic(), create_window(), init_color(), map_rgb_to_bw();
void map_scanline(), put_scanline();

/*
 * Basic magic square for dithering
 */
int dm16[16][16];

/* 
 * Color map, gamma correction map, and lookup tables 
 */
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
double disp_gam = 2.5;		/* default gammas for display and image */
double img_gam = 1.0;
int iflag = 0;			/* flag to tell if gamma was on command line */

int nscan, nrow;		/* size of window */
int nbyte;			/* size of bitrow for 1-bit displays */

int dbg = 0;			/* set if debug mode */

unsigned char *buffer;		/* data storage pointers */

Frame frame;			/* sun window display frame */
Canvas canvas;			/* display frame canvas */
Pixwin *pw;			/* pixwin canvas */


/*****************************************************************
 * TAG( main )
 * 
 * Usage:
 *	getsun [-D] [-{wW}] [-{iI} gamma] [-g gamma] [-l levels] [file]
 * Inputs:
 *	-D:		Debug mode: print input file as read.
 *	-f:		Don't fork after putting image on screen.
 *	-w:		Black & white: reduce color images to B&W before
 *			display.  Advantage is that smoother shading can
 *			be achieved.
 *	-i gamma:	Specify gamma of image. (default 1.0)
 *	-I gamma:	Specify gamma of display image was computed for.
 * 	getsun will also read picture comments from the input file to determine
 *			the image gamma.  These are
 *	image_gamma=	gamma of image (equivalent to -i)
 *	display_gamma=	gamma of display image was computed for.
 *			Command line arguments override values in the file.
 *	-g gamma:	Specify gamma of display. (default 2.5)
 *      -l levels:      Number of color levels.
 *	file:		Input Run Length Encoded file. Uses stdin if not
 *			specified.
 * Outputs:
 * 	Puts image on screen.
 * Assumptions:
 * 	Input file is in RLE format.
 * Algorithm:
 *	[None]
 */

void
main(argc, argv)
int argc;
char **argv;
{
    CONST_DECL char *infname = NULL;
    FILE * infile;
    int gflag = 0;
    int forkflg = 0;

    if ( scanargs( argc, argv,
		   "% Ww%- D%- f%- l%-levels!d Ii%-gamma!F g%-gamma!F file%s",
		   &bwflag, &dbg, &forkflg,
		   &levels, &levels,
		   &iflag, &img_gam,
		   &gflag, &disp_gam,
		   &infname ) == 0 )
	exit( 1 );

	
	if ( levels != 0 && (levels > 6 || levels < 2)) {
		fprintf(stderr,"Level must be in the range [2,6]; %d is invalid!\n", levels);
		exit( 1 );
	}
	if ( iflag == 1 )		/* -i */
		img_gam = 1.0 / img_gam;
	
        infile = rle_open_f("getsun", infname, "r");
    	if ( infile == stdin )
	    infname = "Standard Input";

	if ( !forkflg && fork() != 0 )
	    exit(0);
        get_pic( infile, infname);
        if ( infname != NULL)  fclose( infile);

    	if ( !forkflg )
	{
	    fclose(stdin);		/* Close stdio to release rshd. */
	    fclose(stdout);
	    fclose(stderr);
	}

	window_main_loop(frame);
	exit(0);
}




/*****************************************************************
 * TAG( get_pic )
 * 
 * Read in an RLE image, compute color map, and display it.
 *
 * Inputs:
 *	infile:  File pointer for input.
 *	infname: Name of input file.
 *
 */
void
get_pic( infile, infname, cmdname )
FILE * infile;
char * infname, * cmdname;
{
    register int    i,
                    y;
    int		    ncolors;
    unsigned char  *scan[3];
    rle_hdr hdr;

    /*
     * Read setup info from file. 
     */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmdname, infname, 0 );

    hdr.rle_file = infile;
    rle_get_setup_ok(&hdr, NULL, NULL);

    if ( dbg )
	rle_debug( 1 );

    /* We're only interested in R, G, & B */
    RLE_CLR_BIT(hdr, RLE_ALPHA);
    for (i = 3; i < hdr.ncolors; i++)
	RLE_CLR_BIT(hdr, i);
    ncolors = hdr.ncolors > 3 ? 3 : hdr.ncolors;

    /*
     * Compute image size and allocate storage. 
     */
    nrow = (hdr.xmax - hdr.xmin + 1);
    nscan = (hdr.ymax - hdr.ymin + 1);

    /*
     * Get a window of the right size.
     */
    create_window(nrow, nscan, infname);

    if ( bwflag == 2 )
	nbyte = ((nrow + 15) / 16) * 2;	/* 1 bit display */
    else
	nbyte = ((nrow + 1) / 2) * 2;

    buffer = (unsigned char *) malloc(nbyte * nscan);
    RLE_CHECK_ALLOC( hdr.cmd, buffer, "image memory" );

    /*
     * Set up for rle_getrow.  Pretend image x origin is 0. 
     */
    for (i = 0; i < 3; i++)
	scan[i] = (unsigned char *) malloc(nrow);
    RLE_CHECK_ALLOC( hdr.cmd, scan[0]&&scan[1]&&scan[2], "input scan memory" );
    hdr.xmax -= hdr.xmin;
    hdr.xmin = 0;

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

    init_color();

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
	put_scanline(&buffer[(hdr.ymax - y) * nbyte], nrow, 0,
		     hdr.ymax - y );
    }
    /*
     * Free temp storage 
     */
    for (i = 0; i < 3; i++)
	free(scan[i]);
}




/*****************************************************************
 * TAG( create_window )
 * 
 * Create a sun window and place a pixwin canvas in it.
 *
 * Inputs:
 *	width:  width of window.
 *	height: height of window.
 *	name:   name of image file.
 *
 */
void
create_window( width, height, name)
	int width, height;
	char *name;
{
	char buf[BUFSIZ];

	sprintf(buf,"\"%s\": %d X %d",
		name, height, width);
	frame = window_create(0, FRAME, 
		FRAME_SHOW_LABEL, TRUE,
		FRAME_LABEL, buf,
		FRAME_NO_CONFIRM, TRUE,
		0);
	canvas = window_create(frame, CANVAS,
		WIN_HEIGHT, height,
		WIN_WIDTH, width,
		0);
	window_fit(frame);
	if ((pw = canvas_pixwin(canvas)) == NULL) {
		perror("Cannot create pixwin");
		exit( 1 );
	}
	if (pw->pw_pixrect->pr_depth == 1) bwflag=2;
}




/*****************************************************************
 * TAG( map_scanline )
 *
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
void
map_scanline( rgb, n, s, y, line )
unsigned char *rgb[3], *line;
int n, s, y;
{
    register int i, col, row;

    if ( !bwflag )
    {
	register unsigned char *r, *g, *b;
	for ( row = y % 16, col = 0, i = 0, r = rgb[0], g = rgb[1], b = rgb[2];
	      i < n; i++, r+=s, g+=s, b+=s, col = ((col + 1) & 15) )
	    line[i] = DMAP(in_cmap[0][*r], col, row) +
	    	      DMAP(in_cmap[1][*g], col, row) * levels +
	    	      DMAP(in_cmap[2][*b], col, row) * levelsq;
    }
    else if ( bwflag == 1 )	/* gray scale display */
    {
	register unsigned char *r;

	for ( row = y % 16, col = 0, i = 0, r = rgb[0];
	      i < n; i++, r+=s, col = ((col + 1) & 15) )
	    line[i] = DMAP(*r, col, row);
    }
    else			/* bitmap display */
    {
	register unsigned short *l = (unsigned short *)line;
	register unsigned char * r;

	for ( row = y % 16, col = 0, i = 0, r = rgb[0], *l = 0;
	      i < n; r+=s, col = ((col + 1) & 15) )
	{
	    *l |= (*r > dm16[col][row] ? 0 : 1) << (15-(i % 16));
	    if ( (++i % 16) == 0 )
		*++l = 0;
	}
    }
}




/*****************************************************************
 * TAG( put_scanline )
 * 
 * Output scanline into pixwin.
 *
 * Inputs:
 *	scan:  pointer to scanline.
 *	width: width of scanline.
 *	x,y:   coordinates of this scanline.
 *
 */
void
put_scanline( scan, width, x, y )
	unsigned char *scan;
	int width, x, y;
{
	Pixrect *pix;
	
	if ((pix = mem_point(width,1,pw->pw_pixrect->pr_depth,scan)) == NULL) {
		perror("Unable to allocate pixrect");
		exit( 1 );
	}
	
	pw_write(pw, x, y, width, 1, PIX_SRC, pix, 0, 0);
	free(pix);
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
map_rgb_to_bw( red_row, green_row, blue_row, bw_row, map, rowlen )
rle_pixel *red_row;
rle_pixel *green_row;
rle_pixel *blue_row;
rle_pixel *bw_row;
rle_pixel **map;
int rowlen;
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




/*****************************************************************
 * TAG( init_color )
 * 
 * Build color map and load it into the pixwin.
 *
 */
void
init_color()
{
    register int i; 
    int rgbmap[256][3], bwmap[256]; 
    unsigned char colormap[3][256];
    char buf[12];

    if ( !bwflag )
    {
	/* 
	 * Figure out how many color map entries we can get
	 */
	if ( levels == 0 )
	    levels = 5;		/* default starting point */

	levelsq = levels * levels;
	levelsc = levelsq * levels;
	
	dithermap( levels, disp_gam, rgbmap, divN, modN, dm16 );
	
	/* 
	* Copy the color map entries into something that sunwindows
        * can work with.
	*/
	for(i = 0; i < levelsc; i++) {
		colormap[0][i] = (u_char)rgbmap[i][0]&0xff;
		colormap[1][i] = (u_char)rgbmap[i][1]&0xff;
		colormap[2][i] = (u_char)rgbmap[i][2]&0xff;
	}
	sprintf(buf,"getsun.c%1d",levels);
	pw_setcmsname(pw, buf);
	pw_putcolormap(pw, 0, 4<<levels,colormap[0],colormap[1],colormap[2]);
	return;
    }

    /* Get a B&W color map (gray scale) */
    if ( bwflag == 1 )
    {
	if ( levels == 0 )
		levels = 5;
	levelsq = levels * levels;
	levelsc = levelsq * levels;
	bwdithermap( levels, disp_gam, bwmap, divN, modN, dm16 );

	/* 
	* Copy the color map entries into something that sunwindows
        * can work with.
	*/
    	for ( i = 0; i < levelsc; i++ )
    	{
		colormap[0][i] = (u_char)bwmap[i]&0xff;
		colormap[1][i] = (u_char)bwmap[i]&0xff;
		colormap[2][i] = (u_char)bwmap[i]&0xff;
    	}
	sprintf(buf,"getsun.g%1d",levels);
	pw_setcmsname(pw, buf);
	pw_putcolormap(pw, 0,4<<levels, colormap[0],colormap[1],colormap[2]);
	return;
    }

    /* If b&w 1-bit display, just use two colors */
    if ( bwflag == 2 )
    {
	/* All we care about, really, is the magic square */
	make_square( 255.0, divN, modN, dm16 );
	levels = levelsc = 2;
	return;
    }
}

