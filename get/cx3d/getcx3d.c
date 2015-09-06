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
/* getcx3d.c, 6/24/86, T. McCollough, UU */

#include "rle.h"

#include <stdio.h>

#include <cx3d_types.h>
#include <cx3d_solid.h>

#include "gamma.h"
#include "sig.h"

#define GAMMA	2.5	/* gamma for the display we have attached to the CX */

#define DIRECTCOLG( r, g, b, gv ) (DIRECTCOL( gamma( r, gv ),	\
					      gamma( g, gv ),	\
					      gamma( b, gv ) ))
/* For reading RLE images. */
rle_hdr hdr;

static void go ( pc )
DLMADDR pc;
{
	cx_idle( );
	cx_flush( );
	cx_go( pc, DISP_BM1, IMAGE_VIS, NO, NO );
	cx_flush( );
	cx_glomflush( );
	cx_setpc( pc );
}

static void run ( r, g, b, start, stop, y, magnification, originx, originy,
		  gamma_value )
rle_pixel r, g, b;
int start, stop, y, magnification, originx, originy;
float gamma_value;
{
	/* overlay or clear to background?? */
	if ((hdr.background==1 || hdr.background==2) &&
	    hdr.bg_color[RLE_RED] == r &&
	    hdr.bg_color[RLE_GREEN] == g &&
	    hdr.bg_color[RLE_BLUE] == b) /* screen already good! */ ;
	else /* no overlay, so send the run */
		if (magnification)
			cx_clr_dc( DIRECTCOLG( r, g, b, gamma_value ),
originx+((start-hdr.xmin)*magnification)+hdr.xmin,
originx+((stop-hdr.xmin)*magnification)+hdr.xmin+magnification-1,
originy+((y-hdr.ymin)*magnification)+hdr.ymin,
originy+((y-hdr.ymin)*magnification)+hdr.ymin+magnification-1 );
		else cx_clr_dc( DIRECTCOLG( r, g, b, gamma_value ),
			        originx+start, originx+stop,
			        originy+y, originy+y );
}

static void scanline ( scan, y, magnification, originx, originy, gamma_value )
rle_pixel *scan[];
int y, magnification, originx, originy;
float gamma_value;
{
	register rle_pixel *r = scan[RLE_RED], *g = scan[RLE_GREEN],
	                                          *b = scan[RLE_BLUE];
 	register int start = hdr.xmin;
	DLMADDR	pc = cx_getpc( );
	int runcount = 0;

	/* find and display all runs */
	while (r - scan[0] < hdr.xmax) {
		register rle_pixel _r = *r, _g = *g, _b = *b;

		/* find the run */
		r++, g++, b++;
		while (*r == _r && *g == _g && *b == _b) {
			if (r - scan[0] == hdr.xmax) break;
 			r++, g++, b++;
 		}
		/* send the run */
		run( _r, _g, _b, start, r - scan[0] - 1, y, magnification,
		     originx, originy, gamma_value );
		/* flush the display list before it gets to big.  this number
		   is arbitrary */
		if (runcount++ > 128) {
			go( pc );
			runcount = 0;
		}

		/* the start of the next scanline */
		start = r - scan[0];
	}

	/* do the last pixel on the scanline */
	run( *r, *g, *b, start, start, y, magnification, originx, originy,
	     gamma_value);

	go( pc );
}

/* set the background, if necessary */

static void background ( magnification, originx, originy, gamma_value )
int magnification, originx, originy;
float gamma_value;
{
 	if (hdr.background == 2) {
		DLMADDR pc = cx_getpc( );
		rle_pixel r, g, b;

		switch (hdr.ncolors) {
		case 1: r = g = b = hdr.bg_color[0];
			break;
 		case 3: r = hdr.bg_color[RLE_RED],
 			g = hdr.bg_color[RLE_GREEN],
			b = hdr.bg_color[RLE_BLUE]; break;
		default: fprintf( stderr, "getcx3d: internal error 1\n" );
 		}
		if (magnification)
			cx_clr_dc( DIRECTCOLG( r, g, b, gamma_value ),
originx+hdr.xmin,
originx+((hdr.xmax-hdr.xmin)*magnification)+hdr.xmin+magnification-1,
originy+hdr.ymin,
originy+((hdr.ymax-hdr.ymin)*magnification)+hdr.ymin+magnification-1 );
		else cx_clr_dc( DIRECTCOLG( r, g, b, gamma_value ),
			        originx+hdr.xmin, originx+hdr.xmax,
			        originy+hdr.ymin, originy+hdr.ymax );
		go( pc );
 	}
}

/* load the colormap, if necessary */

static void colormap ( ) {
 	int i;
	DLMADDR pc = cx_getpc( );

	if (hdr.ncmap == 3) {
#ifndef CX3D_COLORMAP
		fprintf( stderr, "getcx3d: (3) colormap not loaded\n" );
		return;
#else
		for (i = 0 ; i < (1 << hdr.cmaplen) ; i++) {
			cx_s_disp_clu( DISP_BM1,
				       DIRECTCOL( i, i, i ),
	      (hdr.cmap[i]>>8)/255.0,
	      (hdr.cmap[i+(1<<hdr.cmaplen)]>>8)/255.0,
	      (hdr.cmap[i+(2<<hdr.cmaplen)]>>8)/255.0 );
			go( pc );
		}
#endif
	} else if (hdr.ncmap == 1) {
#ifndef CX3D_COLORMAP
		fprintf( stderr, "getcx3d: (1) colormap not loaded\n" );
		return;
#else
 		for (i = 0 ; i < (1 << hdr.cmaplen) ; i++) {
			cx_s_disp_clu( DISP_BM1, DIRECTCOL( i, i, i ),
				       (hdr.cmap[i]>>8)/255.0,
				       (hdr.cmap[i]>>8)/255.0,
				       (hdr.cmap[i]>>8)/255.0 );
			go( pc );
		}
#endif
	}
}

static void clear_scan ( scan )
rle_pixel *scan[];
{
	static junk = 1;
	static rle_pixel *r, *g, *b;
	int i;
	
	if (junk) {
		r = (rle_pixel *) malloc( (unsigned) hdr.xmax+1 );
		g = (rle_pixel *) malloc( (unsigned) hdr.xmax+1 );
		b = (rle_pixel *) malloc( (unsigned) hdr.xmax+1 );
		for (i = 0 ; i <= hdr.xmax ; i++)
			r[i] = hdr.bg_color[RLE_RED],
			g[i] = hdr.bg_color[RLE_GREEN],
			b[i] = hdr.bg_color[RLE_BLUE];
		junk = 0;
	}
	bcopy( (char *) r, (char *) scan[RLE_RED], hdr.xmax+1 );
	bcopy( (char *) g, (char *) scan[RLE_GREEN], hdr.xmax+1 );
	bcopy( (char *) b, (char *) scan[RLE_BLUE], hdr.xmax+1 );
}

/* display an rle file using cx3d */

getcx3d ( f, force_background, magnification, originx, originy, gamma_value )
char *f;
int force_background, magnification, originx, originy;
float gamma_value;
{
	FILE *F = rle_open_f_noexit( hdr.cmd, f, "r" );
	rle_pixel *scan[3];
	int y, i;

	if (F == NULL) {
		perror( f );
		return;
	}
	(void)rle_hdr_init( &hdr );
	rle_names( &hdr, hdr.cmd, f, 0 );
	hdr.rle_file = F;
	if (rle_get_setup( & hdr ) < 0) {
		fprintf( stderr,
			 "getcx3d: error reading setup information from %s\n",
			 f ? f : "stdin");
		return;
	}
	if (force_background != -1)
		hdr.background = force_background;
	
	/* we're only interested in r, g, & b */
	RLE_CLR_BIT( hdr, RLE_ALPHA );
	for (i = 3 ; i < hdr.ncolors ; i++) RLE_CLR_BIT( hdr, i );
	
	for (i = 0 ; i < 3 ; i++)
		scan[i] = (rle_pixel *) malloc( (unsigned) hdr.xmax+1 );

	sig_block( );
		background( magnification, originx, originy, gamma_value );
	sig_unblock( );

	sig_block( );
		colormap( );
	sig_unblock( );

	clear_scan( scan );
	while ((y = rle_getrow( & hdr, scan )) <= hdr.ymax) {
		/* deal with b&w images */
		for (i = 2; i >= hdr.ncolors; i--)
			bcopy( (char *) scan[0], (char *) scan[i],
			      hdr.xmax+1);
		/* dump the scanline */
		sig_block( );
			scanline( scan, y, magnification,
				  originx, originy, gamma_value );
		sig_unblock( );
		/* clear it out */
		clear_scan( scan );
	}
	for (i = 0 ; i < 3 ; i++) free( (char *) scan[i] );
}

static void usage ( ) {
	fprintf( stderr,
"usage: getcx3d [-O] [-B] [-d] [-t] [-p x y] [-l] file ...\n" );
	fprintf( stderr, "getcx3d: list as many files as you wish\n" );
	fprintf( stderr, "getcx3d: use - for stdin\n" );
	fprintf( stderr, "getcx3d: see the man page for details\n" );
}

done ( ) {
	/* finish up with the cx */
	sig_block( );
		cx_flush( );
		cx_close( );
		exit( 0 );
}

main ( argc, argv )
int argc;
char *argv[];
{
	int force_background = -1; /* don't force background */
	int doneone = 0;	/* we've not yet displayed an image */
	int magnification = 1; /* we don't want any mag. by default */
	int originx = 0, originy = 0; /* we want origin to be 0 by default */
	float gamma_value = GAMMA; /* gamma for the CX display */

	/* Initialize header. */
	hdr = *rle_hdr_init( (rle_hdr *)NULL );
	rle_names( &hdr, cmd_name( argv ), NULL, 0 );

	/* set up signal stuff */
	sig_setup( );

	/* set up cx */
	sig_block( );
		cx_ack( FALSE );
		if (!cx_open( "/dev/dr0" )) {
			fprintf( stderr, "getcx3d: wait your turn\n" );
			exit( 1 );
		}
		cx_init( );
		cx_init_disp( DISP_BM1 );
	sig_unblock( );

	/* parse command line */
	while (*++argv) if (*argv[0] == '-') switch (argv[0][1]) {
	case 'O': force_background = 1; break;
	case 'B': force_background = 2; break;
	case 'd': magnification = 2; break;
	case 't': magnification = 3; break;
	case 'p': originx = atoi( *++argv ); originy = atoi( *++argv ); break;
	case 'l': gamma_value = 1.0; break;
	case '\0': getcx3d( (char *) 0, force_background, magnification,
			    originx, originy, gamma_value );
		   doneone = 1;
		   /* reset options */
		   force_background = -1;
		   magnification = 1;
		   originx = originy = 0;
		   gamma_value = GAMMA;
		   break;
	default: usage( ); goto out;
	} else {
		getcx3d( argv[0], force_background, magnification,
			 originx, originy, gamma_value );
		doneone = 1; /* we've done one, so forget about stdin */
		/* reset options */
		force_background = -1;
		magnification = 1;
		originx = originy = 0;
		gamma_value = GAMMA;
	}

	if (!doneone) getcx3d( (char *) 0, force_background, magnification,
			       originx, originy, gamma_value );

 out:
	done( );
}
