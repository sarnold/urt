/* 
 * getmex.c - Put RLE images on the Iris display under the window manager.
 * 
 * Author:	Russell D. Fish (Based on getX.c .)
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Nov 19 1986
 * Copyright (c) 1986, University of Utah
 * 
 */

#include <stdio.h>
#include <math.h>
#include "gl.h"
#include "device.h"
#include "rle.h"

#define MAX(i,j)   ( (i) > (j) ? (i) : (j) )
#define MIN(i,j)   ( (i) < (j) ? (i) : (j) )

/* Global variables. */
Colorindex color_start = 512; /* Default color map start of 3/3/3 rgb cube. */
Colorindex bw_start = 128; /* Default color map start of 128 slot grey ramp. */
long window_number;			/* Window number from MEX. */
int x_size, y_size;			/* Size of image. */
int dbg = 0;				/* Set if debug mode. */
int forkflg = 0;			/* Set if not to run in background. */
int bwflag = 0;				/* Set for greyscale output. */
Colorindex *image;			/* Image data buffer pointer. */

#define levels 8			/* Compute 3 bits per channel. */
int modN[256], divN[256];	/* Speed up with precomputed lookup tables. */

int dm16[16][16];	   /* 16x16 magic square, filled in by init_dither. */


/*****************************************************************
 * TAG( main )
 * 
 * Usage:
 *	getmex [-f] [-D] [-w] [file]
 * Inputs:
 * 	-f:	Don't fork after putting image on screen.
 *	-D:	Debug mode: print input file as read.
 *	-w:	Black & white: reduce color images to B&W before display.
 *		Advantage is that smoother shading can be achieved.
 *	-m mapstart	Where to start using the color map.  128 colors are
 *		used for B&W images, starting at 128 by default unless
 *		overridden by this flag.  512 colors are used for color images,
 *		starting at color map location 512.
 *	file:	Input Run Length Encoded file. Uses stdin if not
 *		specified.
 * Outputs:
 * 	Puts image in a window on the screen.
 * Assumptions:
 * 	Input file is in RLE format.
 */

main(argc, argv)
char **argv;
{
    int mapstart, mapflag = 0;
    char *infname = NULL;
    FILE * infile = stdin;

    /* Handle arguments. */
    if ( scanargs( argc, argv, "% f%- w%- D%- m%-mapstart!d file%s\n(\
\tDisplay a URT image dithered in the window system.\n\
\t-f\tKeep program in foreground\n\
\t-w\tDisplay in black and white instead of color.\n\
\t-m\tStarting point in colormap (uses 512 entries for color).\n\
\tLeft mouse pans image, setup key recenters.)",
	&forkflg, &bwflag, &dbg, &mapflag, &mapstart, &infname ) == 0 )
	    exit( 1 );
    if ( mapflag )
	if ( bwflag ) bw_start = mapstart; else color_start = mapstart;
    infile = rle_open_f(cmd_name(argv), infname, "r");

    init_dither();		/* Set up the dither matrices. */
    /* Read image and make a window for it. */
    get_pic( infile, infname, cmd_name(argv) );
    update_pic();		/* Keep drawing the window. */
}

/* 
 * Read an image from the input file and display it.
 */
get_pic( infile, infname, cmdname )
FILE * infile;
char * infname, cmdname;
{
    register int i, y;
    int ncolors;
    unsigned char *scan[3];
    rle_hdr hdr;

    /*
     * Read setup info from file. 
     */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmdname, infname, 0 );
    hdr.rle_file = infile;
    rle_get_setup_ok( &hdr, NULL, NULL );

    if ( dbg )
	rle_debug( 1 );

    /* We`re only interested in R, G, & B */
    RLE_CLR_BIT(hdr, RLE_ALPHA);
    for (i = 3; i < hdr.ncolors; i++)
	RLE_CLR_BIT(hdr, i);
    ncolors = hdr.ncolors > 3 ? 3 : hdr.ncolors;
    if ( ncolors == 1 ) bwflag = TRUE;

    /*
     * Compute image size and allocate storage for colormapped image. 
     */
    x_size = (hdr.xmax - hdr.xmin + 1);
    y_size = (hdr.ymax - hdr.ymin + 1);
    image = (Colorindex *) malloc(x_size * y_size * sizeof( Colorindex ));

    /*
     * Set up for rle_getrow.  Pretend image x origin is 0. 
     */
    for (i = 0; i < 3; i++)
	scan[i] = (unsigned char *) malloc(x_size);
    hdr.xmax -= hdr.xmin;
    hdr.xmin = 0;

    /* For each scan line, dither it into the image memory. */
    while ((y = rle_getrow(&hdr, scan)) <= hdr.ymax)
    {
	if ( bwflag && ncolors > 1 )
	{
	    rgb_to_bw( scan[0], scan[1], scan[ncolors - 1], scan[0], x_size );
	    /* Note: map_scanline only uses channel 0 for B&W */
	}

	map_scanline( scan, x_size, y,
		      &image[(y - hdr.ymin) * x_size] );
    }

    /*
     * Free temp storage 
     */
    for (i = 0; i < 3; i++)
	free(scan[i]);

    /*
     * Get a window of the right size (user positions it with the mouse). 
     */
    if ( forkflg ) foreground();	/* Don`t fork. */
    maxsize( x_size, y_size );
    window_number = winopen( "getmex" );
    if ( infname ) wintitle( infname );

    makemap();				/* Initialize the Iris color map. */

    qdevice( REDRAW );
    qdevice( LEFTMOUSE );	      /* Pan the image under mouse control. */
    qdevice( SETUPKEY );		/* Reset panning. */
    unqdevice( INPUTCHANGE );		/* We don`t pay attention to these. */

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
    int window_x_size, window_y_size, window_x_origin, window_y_origin;
    int x_min, x_max, y_min, y_max, x_start, y_start, x_end, y_end, x_len;
    int x_origin, y_origin, new_x_center, new_y_center;
    int x_center, y_center, saved_x_center, saved_y_center;

    register int y;
    register Colorindex *y_ptr;

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
	    case REDRAW:
		winset( window_number );
		reshapeviewport();
		color( 22 );
		clear();

		/* Lower left corner of screen, in image coordinates.
		 * (Keep the center of the image in the center of the window.)
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
		for ( y = y_start, y_ptr = image + y_start*x_size + x_start;
		      y <= y_end && (y%16 != 0 || qtest() != REDRAW);
		      y++, y_ptr += x_size )
		{
		    cmov2i( x_start, y );
		    writepixels( x_len, y_ptr );
		}
		break;

	    /* Setup key - Reset viewing to look at the center of the image.
	     * Shift-Setup - Restores a saved view center.
	     * Control-Setup - Saves the current view center for Shift-Setup.
	     */
	    case SETUPKEY:
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
map_scanline( rgb, n, y, line )
unsigned char *rgb[3];
Colorindex *line;
{
    register int i, col;
    int row, *row_ptr;

#   define DMAP(v,x) (modN[v]>row_ptr[x] ? divN[v] + 1 : divN[v])

    row = y % 16;
    row_ptr = dm16[ row ];	       /* Cache ptr to row of dither array. */

    if ( !bwflag )			/* Color display. */
    {
	register unsigned char *r, *g, *b;
	for ( col = 0, i = 0, r = rgb[0], g = rgb[1], b = rgb[2];
	      i < n; i++, r++, g++, b++, col = ((col + 1) & 15) )
	{
	    /* Combine 3 3-bit colors into a 3/3/3 color map index. */
	    line[i] = color_start + DMAP(*r, col) + (DMAP(*g, col)<<3) +
		(DMAP(*b, col)<<6);
	}
    }
    else				/* Gray scale display. */
    {
	register unsigned char *bw;

	for ( i = 0, bw = rgb[0]; i < n; i++, bw++ )
	    line[i] = bw_start + (*bw>>1);
    }
}

#if 0
/* From Kitaoka's dithering code */
dmap(v,x,y)
	register v;
    int x,y ;
{
    int d ;

	v = gammamap[v];
    d = dm4[x%4][y%4] ;
    if ((v%51)>d) return (v/51)+1 ; else return (v/51) ;
}
#endif

init_dither()				/* Miscellaneous global setup. */
{
    int i, j, k, l, planes, pixels[216];
    unsigned int value;
    float N;

    make_square( 255.0 / (levels - 1), divN, modN, dm16 );
}

/** NOTE: This is the makemap program from /usr/people/gifts/mextools/tools,
 ** with the gamma and getset routines from /usr/people/gifts/mextools/portlib
 ** appended.  The only modification is that it only does the part of the map
 ** required here, and expands the RGB map to 512 colors.
 **/
/*
 *		Make the color map using the clues provided by ~/.desktop
 *		most of the mextools need the colors mapped by this program.
 *
 *				Paul Haeberli - 1984
 *
 */
/** #include "gl.h" **/
/** #include "gl2/port.h" **/

float getgamma();

makemap()
{
    register int i, j, v;
    register int r, g, b, w;
    int planes;

    planes = getplanes();

/* if there are more than 8 planes make a ramp at 128 */
/** Position as specified, and only do it if showing a b&w image. **/
    if (planes > 8 /**>**/ && bwflag /**<**/ ) 
	for (i=0; i<128; i++) {
	    gammapcolor(i+ /**128>**/ bw_start /**<**/,i<<1,i<<1,i<<1);
	}

/* if there are more than 8 planes make an ordered color map at 256 */
/** but if more than 9 planes, make the map at 512, 512 colors long. **/
    if (planes > 9 /**>**/ && ! bwflag /**<**/ )	/** => **/
	for (i=0; i<512; i++) {
	    r = (i>>0) & 7;
	    g = (i>>3) & 7;
	    b = (i>>6) & 7;
	    r = (255*r)/7;
	    g = (255*g)/7;
	    b = (255*b)/7;
	    gammapcolor(i+ /**512>**/ color_start /**<**/,r,g,b);
    	}
}

/*
 *	gamma - 
 *		Some support for gamma correction when reading and writing
 *    		color map entries.
 *
 *				Paul Haeberli - 1984
 *
 */
/** #include "math.h" **/
/** #include "port.h" **/
/** #include "gl.h" **/
/** #include "stdio.h" **/

FILE *configopen();

float gammacorrect();
float ungammacorrect();

static unsigned char rgamtable[256];
static unsigned char ggamtable[256];
static unsigned char bgamtable[256];
static unsigned char rungamtable[256];
static unsigned char gungamtable[256];
static unsigned char bungamtable[256];
static short firsted;

gammapcolor(index,r,g,b)
register int index,r,g,b;
{
	short i;

	if (!firsted) {
		readgamtables();
		firsted++;
	}
	r = rgamtable[r&0xff];
	g = ggamtable[g&0xff];
	b = bgamtable[b&0xff];
	mapcolor(index,r,g,b);
}

static makegamtables()
{
	register float gamval;
	register float val;
	register short i;
	int rbal, gbal, bbal; 

	gamval = getgamma();
	getcolorbal(&rbal,&gbal,&bbal);
	for (i=0; i<256; i++) {
		rgamtable[i] = 255*gammacorrect((rbal*i)/(255.0*255.0),gamval);
		ggamtable[i] = 255*gammacorrect((gbal*i)/(255.0*255.0),gamval);
		bgamtable[i] = 255*gammacorrect((bbal*i)/(255.0*255.0),gamval);
	}
	bzero(rungamtable,256);
	bzero(gungamtable,256);
	bzero(bungamtable,256);
	for (i=0; i<256; i++) {
		rungamtable[rgamtable[i]] = i;
		gungamtable[ggamtable[i]] = i;
		bungamtable[bgamtable[i]] = i;
	}
	fixup(rungamtable);
	fixup(gungamtable);
	fixup(bungamtable);
}

static fixup(cptr)
register unsigned char *cptr;
{
	register short i, lowval;

	for (i=256; i--; ) {
		if (*cptr == 0) 
			*cptr = lowval;
		else
			lowval = *cptr;
	}
}

gamgetmcolor(index,r,g,b)
int index;
unsigned short *r, *g, *b;
{
	static short firsted;
	unsigned short tr, tg, tb;

	if (!firsted) {
		readgamtables();
		firsted++;
	}
	getmcolor(index,&tr,&tg,&tb);
	*r = rungamtable[tr&0xff];
	*g = gungamtable[tg&0xff];
	*b = bungamtable[tb&0xff];
}

float gammacorrect( i, gamma)
float i, gamma;
{
    return pow(i,1.0/gamma);
}

float ungammacorrect( i, gamma)
float i, gamma;
{
    return pow(i,gamma);
}

newgamma()
{
    firsted = firsted = 0;
}

newgamtables()
{
    FILE *outf;

    if ((outf = configopen(".gamtables","w")) == 0) {
	fprintf(stderr,"couldn't open .gamtables\n");
	return;
    }
    makegamtables();
    fwrite(rgamtable,256,1,outf);
    fwrite(ggamtable,256,1,outf);
    fwrite(bgamtable,256,1,outf);
    fwrite(rungamtable,256,1,outf);
    fwrite(gungamtable,256,1,outf);
    fwrite(bungamtable,256,1,outf);
    fclose(outf);
}

readgamtables()
{
    FILE *inf;

    if ((inf = configopen(".gamtables","r")) == 0)  {
	newgamtables();
	if ((inf = configopen(".gamtables","r")) == 0)  {
	    fprintf(stderr,"couldn't open .gamtables\n");
	    return;
	}
    }
    fread(rgamtable,256,1,inf);
    fread(ggamtable,256,1,inf);
    fread(bgamtable,256,1,inf);
    fread(rungamtable,256,1,inf);
    fread(gungamtable,256,1,inf);
    fread(bungamtable,256,1,inf);
    fclose(inf);
}

/*
 *	getset - 
 *		Get and set values stored in ~/.desktop and ~/.gamma
 *
 *				Paul Haeberli - 1984
 *
 */
/** #include "stdio.h" **/
/** #include "port.h" **/
/** #include "gl.h" **/

FILE *configopen();

/** savecolors and restorecolors **/

float getgamma()
{
    FILE *gamfile;
    float gam;

    if ((gamfile = configopen(".gamma","r")) )  {
        if (fscanf(gamfile,"%f\n",&gam) == 1) {
	    fclose(gamfile);
	    return gam;
	} else 
	    fclose(gamfile);
    }
    return 2.2;
}

setgamma( gam )
float gam;
{
    FILE *gamfile;

    if ((gamfile = configopen(".gamma","w")) == 0) {
	fprintf(stderr,"couldn't open .gamma\n");
	return;
    }
    fprintf(gamfile,"%f\n",gam);
    fclose(gamfile);
    newgamtables();
}

getcolorbal(r,g,b)
unsigned int *r, *g, *b;
{
    FILE *cbfile;

    if ((cbfile = configopen(".cbal","r")) ) { 
        if (fscanf(cbfile,"%d %d %d\n",r,g,b) == 3) {
	    if (*r>255)
		*r = 255;	
	    if (*g>255)
		*g = 255;	
	    if (*b>255)
		*b = 255;	
            fclose(cbfile);
            return;
        } else 
            fclose(cbfile);
    }
    *r = 255;
    *g = 255;
    *b = 255;
    return;
}

setcolorbal(r,g,b)
int r, g, b;
{
    FILE *cbfile;

    if ((cbfile = configopen(".cbal","w")) == 0) {
	fprintf(stderr,"couldn't open .cbal\n");
	return;
    }
    fprintf(cbfile,"%d %d %d\n",r,g,b);
    fclose(cbfile);
    newgamtables();
}

FILE *configopen( name, mode )
char name[];
char mode[];
{
    char homepath[100];
    FILE *f;
    char *cptr;

    cptr = (char *)getenv("HOME");
    if (!cptr)
	return 0;
    strcpy(homepath,cptr);
    strcat(homepath,"/");
    strcat(homepath,name);
    return fopen(homepath,mode);
}
