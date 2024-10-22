/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notices are 
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
 * getx11.c - Put RLE images on X display.
 *
 * Author:	Spencer W. Thomas  (x10)
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Feb 20 1986
 * Copyright (c) 1986, University of Utah
 *
 * Modified:	Andrew F. Vesper (x 11)
 *		High Performance Workstations
 *		Digital Equipment Corp
 * Date:	Fri, Aug 7, 1987
 *		Thu, Jan 14, 1988
 * Copyright (c) 1987,1988, Digital Equipment Corporation
 *
 * Modified:	Martin R. Friedmann (better X11, flipbook, mag, pix_info)
 * 		Dept of Electrical Engineering and Computer Science
 *		University of Michigan
 * Date:	Tue, Nov 14, 1989
 * Copyright (c) 1989, University of Michigan
 */

#include <unistd.h>
#include "getx11.h"

/* Make Sys V macros map to BSD macros */
#ifndef _tolower
#define _tolower tolower
#define _toupper toupper
#endif


/* must be able to fit 3 * log2 (MAXIMUM_LEVELS) bits in a 32 bit word */

#define DEFAULT_LEVELS		240
#define MAXIMUM_LEVELS		1024

static int get_pic ();
static void update_pic();

/*
 * Global variables
 */

double		display_gamma = 2.5;	/* default gamma for display */
int		iflag = 0;
int		red_shift;
int		green_shift;
int		blue_shift;
Pixel		red_mask;
Pixel		green_mask;
Pixel		blue_mask;
Pixel		pixel_base;

char 		*progname = NULL;
Display		*dpy = NULL;
Window		root_window = 0;
int		screen = 0;

Boolean		debug_flag = False;	/* set if debug mode -D */
Boolean		verbose_flag = False;	/* set if verbose mode -v */
int 		jump_flag = 0, jump_scans = 0, stingy_flag = 0;
int             specified_levels = 0;
#ifdef X_SHARED_MEMORY
Boolean		do_sharing = True;
Boolean		use_shared_pixmaps = False;
Boolean		no_shared_space = False;
#endif
Boolean		no_color_ref_counts;	/* X11/NeWS 2.0 server BUG */

static struct {
    CONST_DECL char	*string;
    int	type;
}  visual_type_table[] = {
    
{ "staticgray", StaticGray},
{ "grayscale", GrayScale},
{ "staticgrey", StaticGray},
{ "greyscale", GrayScale},
{ "staticcolor", StaticColor},
{ "pseudocolor", PseudoColor},
{ "truecolor", TrueColor},
{ "directcolor", DirectColor}
};


image_information *img_info = NULL;
void init_img_info( i )
image_information *i;
{
    i->window = i->icn_window = 0;
    i->pixmap = i->icn_pixmap = 0;
    i->pix_info_window = 0;
    i->gc = i->icn_gc = NULL;
    i->image = i->icn_image = NULL;
    i->colormap = 0;
    i->visual_class = -1;
    i->pixmap_failed = False;

    i->filename = NULL;
    i->img_num = 0;
    i->title = NULL;
    i->fd = stdin;
    i->img_channels = 0;
    i->dpy_channels = 0;
    i->scan_data = NULL;
    i->map_scanline = NULL;
    i->MAG_scanline = NULL;
    i->gamma = 0.0;
    
    i->x = i->y = 0;
    i->xo = i->yo = 0;
    i->w = i->h = 0;
    i->win_w = i->win_h = 0;
    i->icn_w = i->icn_h = 0;
    i->pix_w = i->pix_h = 0;
    i->icn_factor = 1;

    i->pan_x = i->pan_x = 0;
    i->pan_w = i->pan_h = 0;
    i->mag_fact = 1;
    i->save_mag_fact = 1;
    i->mag_mode = False;
    
    i->binary_img = False;
    i->dither_img = False;
    i->mono_img = False;
    i->rw_cmap = False;
    i->sep_colors = False;
    i->mono_color = False;
    i->color_dpy = True;

    i->in_cmap = NULL;
    i->ncmap = 0;
    i->cmlen = 0;
    i->modN = NULL;
    i->divN = NULL;
    i->dm16 = NULL;
    i->pixel_table = NULL;
    i->x_cmap.red_mask = i->x_cmap.green_mask = i->x_cmap.blue_mask = 1;
    i->x_cmap.red_shift = i->x_cmap.green_shift = i->x_cmap.blue_shift = 0;
    i->x_cmap.pixel_base = 0;
    i->black_pixel = 0;
    i->white_pixel = 1;
#ifdef X_SHARED_MEMORY
    i->shm_img.shmid = -1;
    i->shm_pix.shmid = -1;
#endif
}


/*****************************************************************
 * TAG( main )
 * 
 * usage : getx11 [-= window-geometry] [-a] [-d display] [-D] [-f] [-T]
 * 		  [-g gamma] [-{iI} gamma] [-j [jump-scans]]
 * 		  [-m [maxframes/sec]] [-n levels]
 *       	  [-s] [-t title] [-v] [-{wW}] [-x visual] [file ...]
 * Inputs:
 * 	-f:		Dont fork after putting image on screen.
 *	-D:		Debug mode: print input file as read.
 *	-W:		bitonal; binary black and white
 *	-w:		Black & white: reduce color images to monochrome before
 *			display.  Advantage is that smoother shading can
 *			be achieved.
 *	-v:		Print verbose window type information.
 *	-d display:	Specify display name.
 *	-= window_geometry:
 *			Specify window geometry (but min size set by file).
 *	-i gamma:	Specify gamma of image. (default 1.0)
 *	-I gamma:	Specify gamma of display image was computed for.
 *	-g gamma:	Specify gamma of display. (default 2.5)
 *	-x vis_type:    Specify visual type to be used (string or int, 0 to 5)
 *	-n levels:	number of levels to display of each color
 *	-a:		Do not dither
 *	-s:		Be stingy about memory/pixmap usage.
 *	-S:		Don't share memory, even if it can.
 *	-j:		Display every jump-scans scanlines.
 *	-m:		Use movie mode.  flip_book frames per second
 *	-T:		Use TV mode... (like movie mode but no playback)
 *
 *	file:		Input Run Length Encoded file. Uses stdin if not
 *			specified.
 *
 * 	getx11 will also read picture comments from the input file to determine
 *			the image gamma.  These are
 *
 *	image_gamma=	gamma of image (equivalent to -i)
 *	display_gamma=	gamma of display image was computed for.
 *	image_title=	title to name images X window.
 *	title =		same...
 *		
 *	Any command line arguments override the values in the file.
 *		
 * Outputs:
 * 	Puts image on X11 screen.
 * Assumptions:
 * 	Input file is in RLE format.
 * Algorithm:
 *	[None]
 */

void
main (argc, argv)
int		argc;
char 		** argv;
{
    char	** infnames = NULL;
    char	* display_name = NULL;
    char	* window_geometry = NULL;
    int		n_malloced, num_imgs = 0, using_stdin;
    int		levels;
    int		nfile = 0, forkflg = 0, dflag = 0, gflag = 0, wflag = 0;
    int		vflag = 0, aflag = 0, bin_flag = 0, mono_flag = 0;
    int		quit_flag = 0, tflag = 0, TVflag = 0;
    int		flip_book = 0;
    int		flip_book_frams_sec = 30;
    int		sharing_flag = False;
    int		tv_delay = 0;
    char        *tv_command = NULL;
    char	*vis_type = NULL, *title_string = NULL;
    int		visual_type = -1;
    int		status, done = 0, rle_cnt = -1;
    image_information *img, *previous_img;
    double	image_gamma = 0.0;	/* default gamma for image */

    progname = argv[0];

    if ( scanargs( argc, argv,
	  "% =%-window-geometry!s a%- d%-display!s D%- f%- \n\
	g%-gamma!F iI%-gamma!F j%-jump-scans%d m%-maxframes/sec%d n%-levels!d \n\
	q%- s%- S%- t%-title!s T%-delay%dcommand%s v%- wW%- x%-visual!s file%*s",
		   &wflag, &window_geometry,
		   &aflag,
		   &dflag, &display_name,
		   &debug_flag,
		   &forkflg,
		   &gflag, &display_gamma,
		   &iflag, &image_gamma,
		   &jump_flag, &jump_scans,
		   &flip_book, &flip_book_frams_sec,
		   &specified_levels, &specified_levels,
		   &quit_flag,
		   &stingy_flag,
		   &sharing_flag,
		   &tflag, &title_string,
		   &TVflag, &tv_delay, &tv_command,
		   &verbose_flag,
		   &bin_flag,
		   &vflag, &vis_type,
		   &nfile, &infnames ) == 0 )
	exit(1);

    if (TVflag && flip_book) {
	fprintf(stderr, "%s: -T and -m cannot be used together\n", progname);
	exit(1);
    }

    /* -S means don't share */
#ifdef X_SHARED_MEMORY
    if ( !sharing_flag )
	do_sharing = False;
#endif

    /* Check for -w instead of -W. */
    if ( bin_flag == 2 )
    {
	bin_flag = 0;
	mono_flag = 1;
    }

    using_stdin = ( nfile ? False : True );
    n_malloced = ( using_stdin ? 1 : nfile );

    img_info = (image_information *)malloc(n_malloced *
					   sizeof(image_information) );

    if ( img_info == NULL ){
	perror("malloc returned NULL");
	exit (1);
    }
    
    if ( vflag != 0) {
	if (isdigit (vis_type[0]) ) {
	    visual_type = atoi (vis_type);
	}
	else {
	    register char *ptr;
	    register int  i;
	    visual_type = 9999;
	    
	    for (ptr = vis_type; *ptr != '\0'; ptr++) {
		if (isupper(*ptr)) *ptr = _tolower(*ptr);
	    }

	    for (i = 0; i < COUNT_OF (visual_type_table); i++) {
		if (strcmp (visual_type_table[i].string, vis_type) == 0) {
		    visual_type = visual_type_table[i].type;
		    break;
		}
	    }
	}
	
	if ( visual_type < 0 || visual_type > 5 ) {
	    fprintf ( stderr, "Bad visual type %s, ignoring request\n",
		      vis_type);
	    visual_type = -1;
	}
    }
    
    levels = specified_levels;
    if (levels == 0) levels = DEFAULT_LEVELS;	/* default starting point */
    if (levels > MAXIMUM_LEVELS) levels = MAXIMUM_LEVELS;
    
    /*
     * open the display
     */
    
    open_x_display ( display_name );

    /* 
     * For each file, display it.
     */
    
    do {
	/* get more space, but in TV mode get only two img_infos */
	if ( num_imgs >= n_malloced && (! TVflag || num_imgs == 1) )
	    img_info = (image_information *)
		realloc( img_info,
			 ++n_malloced * sizeof(image_information) );
	if ( img_info == 0 )
	{
	    fprintf(stderr, "%s: Out of memory!\n", progname);
	    exit( RLE_NO_SPACE );
	}
	if ( TVflag && num_imgs > 0 ) 
	    img = &img_info[1];
	else
	    img = &img_info[num_imgs];

	init_img_info( img );

	/* we need pixmaps for movie mode... */
	if ( stingy_flag && !flip_book )
	    img_info[0].pixmap_failed = True;

	if ( iflag == 2 && image_gamma != 0.0 ) img->gamma = 1.0 / image_gamma;
	else if ( iflag == 1 ) img->gamma = image_gamma;

	img->binary_img = ( bin_flag ? True : False );
	img->mono_img = ( mono_flag || bin_flag ? True : False );
	if ( img->mono_img )
	    img->color_dpy = False;
	img->dither_img = ( aflag == 0 );
	img->visual_class = visual_type;
	img->lvls = levels;
	img->lvls_squared = levels * levels;

	if ( img->mono_img )
	    img->dpy_channels = 1;

	if ( (flip_book || TVflag) && img != &img_info[0] )
	{
	    previous_img = img - 1;
#define INHERIT( thing ) img->thing = previous_img->thing
	    INHERIT(window);
	    INHERIT(win_w);
	    INHERIT(win_h);
	    if (TVflag)
	    {
		INHERIT(pixmap);
#ifdef X_SHARED_MEMORY
		if ( use_shared_pixmaps )
		    INHERIT(shm_pix);
#endif
	    }
	    if (previous_img->pixmap || TVflag)
	    {
		INHERIT(image);
#ifdef X_SHARED_MEMORY
		if ( use_shared_pixmaps )
		    INHERIT(shm_pix);
#endif
	    }
	    INHERIT(icn_window);
	    INHERIT(icn_pixmap);
	    INHERIT(gc);
	    INHERIT(icn_gc);
	    INHERIT(pixmap_failed);
	    INHERIT(divN);
	    INHERIT(modN);
	    INHERIT(dm16);
	    INHERIT(pixel_table);
	    INHERIT(lvls);
	    INHERIT(lvls_squared);
	    INHERIT(colormap);
	    INHERIT(visual_class);
	    INHERIT(dpy_depth);
	    INHERIT(dpy_channels);
	    INHERIT(sep_colors);
	    INHERIT(rw_cmap);
	    INHERIT(dither_img);
	    INHERIT(color_dpy);
	    INHERIT(mono_img);
	    INHERIT(binary_img);
	    INHERIT(x_cmap);
	    INHERIT(white_pixel);
	    INHERIT(black_pixel);
#undef INHERIT
	}
	else previous_img = 0;

	if ( !using_stdin ) {
	    /* rle_cnt will be set to -1 when it is time to open a new
	     * file.
	     */
	    if ( rle_cnt < 0 )
	    {
		if ( strcmp( *infnames, "-" ) == 0 )
		{
		    img->filename = "Standard Input";
		    img->fd = stdin;
		}
		else
		{
		    img->filename = *infnames;

		    img->fd = rle_open_f_noexit( "getx11", img->filename, "r");
	    
		    if (img->fd == NULL) {
			if ( nfile == 1 )
			    exit (1);
			else {
			    nfile--; infnames++;
			    continue;
			}
		    }
		}
		rle_cnt = 0;
	    }
	    else
	    {
		/* Safe to copy from previous image because this is at
		 * least the second one in this file.
		 */
		img->filename = img[-1].filename;
		img->fd = img[-1].fd;
	    }
	}
	else {
	    img->filename = "Standard Input";
	    img->fd = stdin;
	    if ( rle_cnt < 0 )
		rle_cnt = 0;
	}
	img->img_num = rle_cnt;

	if ( tflag && title_string )
	    img->title = title_string;

	status = get_pic( img, window_geometry, previous_img );

	switch (status){
	case SUCCESS:
	    num_imgs++;
	    rle_cnt++;
	    break;

	case MALLOC_FAILURE:
	case RLE_NO_SPACE:
	    fprintf(stderr,"%s: Out of Memory! Trying to continue\n",progname);
	    if ( !using_stdin ) 
	    {
		fclose( img->fd );
		nfile--;
		infnames++;
	    }
	    rle_cnt = -1;
	    break;
	    
	case RLE_EMPTY:
	case RLE_EOF:
	case RLE_NOT_RLE:
	    if ( status == RLE_NOT_RLE || rle_cnt == 0 )
		rle_get_error( status, progname, img->filename );
	    if ( using_stdin )
		done = 1;
	    else
	    {
		fclose( img->fd );
		nfile--;
		infnames++;
		if ( nfile == 0 )
		    done = 1;
	    }
	    rle_cnt = -1;
	    break;
	    
	case FATAL_FAILURE:	
	    if ( flip_book )
		fprintf(stderr,"%s: Cannot flip-book, sorry...\n", progname);
	    exit(1);
	    break;
	}
	if ( TVflag && !done ) {
	  if ( tv_command ) {
	    system( tv_command );
	  }
	  if ( tv_delay ) {
	    sleep( tv_delay );
	  }
	}
    } while( !done );
    
    if ( TVflag ) {
	if ( img_info->scan_data ) {
	    free( img_info->scan_data );
	    img_info->scan_data = NULL;
	}
	num_imgs = 1;
	flip_book = False;
    }

    /* Quit flag means quit after displaying all the images. */
    if ( quit_flag )
    {
	XCloseDisplay( dpy );
	exit( 0 );
    }
    
#ifdef unix
    if ( ! forkflg ) {
	
	fflush( stderr );
	if ( fork() == 0 ) {
	    close( 0 );
	    close( 1 );
	    close( 2 );
	    /* Set process group to avoid signals under sh. */
#ifdef SYS_V_SETPGRP
	    (void)setpgrp();
#else
	    (void)setpgrp( 0, getpid() );
#endif
	    update_pic ( img_info, num_imgs, flip_book, flip_book_frams_sec );
	}
	else exit( 0 );
    }
    else 
#endif
	update_pic ( img_info, num_imgs, flip_book, flip_book_frams_sec );

    XCloseDisplay( dpy );
    exit (0);
}

void
handle_exposure( img, x, y, width, height, img_h )
register image_information *img;
int x, y, width, height, img_h;
{
    /* imaged width and imaged height...  May be less that windows size */
    int iw, ih;
    int xo = img->xo;
    int yo = img->yo; 

    iw = img->w * img->mag_fact;
    ih = img->h * img->mag_fact;
    
    /* clear left side */
    if ( xo && (x < xo) && width > 0 && height > 0) {
	int w = xo - x;
	w = Min(width, w);
	XClearArea( dpy, img->window, x, y, w, height, False );
	x = xo;
	width -= w;
    }

    /* Clear Top part */
    if ( yo && (y < yo) && width > 0 && height > 0) {
	int h = yo - y;
	h = Min(height, h);
	XClearArea( dpy, img->window, x, y, width, h, False );
	y = yo;
	height -= h;
    }

    /* Clear right side */
    if ( iw < img->win_w ) 
	if (x + width > xo + iw && height > 0) {
	    int w = x + width - xo - iw;
	    XClearArea( dpy, img->window, xo + iw, y, w, height, False );
	    width -= w;
	}	

    /* Clear Bottom side */
    if ( ih < img->win_h ) 
	if (y + height > yo + ih && width > 0) {
	    int h = y + height - yo - ih;
	    XClearArea( dpy, img->window, x, yo + ih, width, h, False );
	    height -= h;
	}	
    
    if ( width <= 0 || height <= 0 )
	return;

    ih = Min( img->win_h, img->h );

    /* HACK if the image has not yet read itself in, dont blit any of it    */
    /* instead clear out that top portion of the window(not needed oh well) */
    if (!img->mag_mode)
	if (y - yo < ih - img_h && width > 0) {
	    int h = (ih - img_h) - ( y - yo );
	    XClearArea( dpy, img->window, x, y, width, h, False );
	    height -= h;
	    y = ih - img_h + yo;
	}
    
    /*
     * if bitmap, round beginning pixel to beginning of word
     */
	
    if ( img->binary_img ) {
	int offset = x % BitmapPad (dpy);
	x -= offset;
	width += offset;
    }
    
    if ( width <= 0 || height <= 0 )
	return;

    if ( img->pixmap )
	XCopyArea(dpy, img->pixmap, img->window, img->gc,
		  x-xo, y-yo, width, height, x, y );
    else
	put_X_image(img, img->window, img->gc, img->image,
		  x-xo, y-yo, x, y, width, height );
    return;
}

/* 
 * Read an image from the input file and display it.
 */
static int
get_pic( img, window_geometry, previous_img )
register image_information *img, *previous_img;
char *window_geometry;
{
    int		image_xmax, image_ymax;
    int		x11_y, scan_y, view_w, view_h;
    unsigned char		*save_scan[3], *read_scan[3];
    register int		i;
    register int		image_y;
    register int		y_base, lines_buffered, lines_blitted;
    int 			rle_err;
    int 			buffer_scans = jump_scans;
    rle_hdr			img_hdr;

    /*
     * Read setup info from file. 
     */
    img_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    img_hdr.rle_file = img->fd;
    
    rle_names( &img_hdr, "getx11", img->filename, 0 );
    if ( (rle_err = rle_get_setup(&img_hdr)) < 0) 
	return ( rle_err );
    
    image_xmax = img_hdr.xmax;
    image_ymax = img_hdr.ymax;
    
    img->x = img_hdr.xmin;
    img->y = img_hdr.ymin;
    img->w = image_xmax - img_hdr.xmin + 1;
    img->h = image_ymax - img_hdr.ymin + 1;
    
    img_hdr.xmin = 0;
    img_hdr.xmax = img->w - 1;
    
    /* Were only interested in R, G, & B */
    
    RLE_CLR_BIT(img_hdr, RLE_ALPHA);
    for (i = 3; i < img_hdr.ncolors; i++)
	RLE_CLR_BIT(img_hdr, i);
    
    img->dpy_channels = img->img_channels = (img_hdr.ncolors > 3 ) ?
	3 : img_hdr.ncolors;
    
    if ( img->img_channels == 1 ) 
	img->mono_img = True;
    if ( img->mono_img )
	img->dpy_channels = 1;

    check_mono_color( img, &img_hdr );

    /* jump/buffer scans according to the -j flag and to img->fd == sdtin */
    buffer_scans = (jump_flag ?
		    ((jump_scans > 0) ? jump_scans : img->h) :
		    ((img->fd == stdin) ? 1 : img->h));

    if ( !previous_img )
	/* find a suitable visual for the image */
	find_appropriate_visual( img );

    if ( img->img_channels < img->dpy_channels )
	fprintf(stderr, "%s: dpy_channels > img_channels, %d > %d\n", progname,
 		img->dpy_channels, img->img_channels);

    get_dither_colors( img, &img_hdr );

    if ( !previous_img )
	/* Get X color map */
	init_color( img );

    if ( !img->title )
	if ( (img->title = rle_getcom("image_title", &img_hdr )) == NULL &&
	     (img->title = rle_getcom("IMAGE_TITLE", &img_hdr )) == NULL &&
	     (img->title = rle_getcom("title", &img_hdr )) == NULL &&
	     (img->title = rle_getcom("TITLE", &img_hdr )) == NULL )
	{
	    if ( img->img_num > 0 )
	    {
		char *buf = (char *)malloc( strlen( img->filename ) + 8 );
		sprintf( buf, "%s(%d)", img->filename,
			 img->img_num + 1 );
		img->title = buf;
	    }
	    else
		img->title = img->filename;
	}

    /*
     * Here if we are flip_booking we gotta get nasty here... img->w, img->h
     * and img->img_channels must match for well surely be screwed if they
     * dont match up  and we flip_book
     */
    if ( previous_img )
    {
	if ( img->w != previous_img->w || img->h != previous_img->h ||
	    img->img_channels != previous_img->img_channels )
	{
	    fprintf( stderr,
		    "%s: Images %s and %s dont match in size or channels\n",
		    progname, previous_img->title, img->title );
	    return( FATAL_FAILURE );
	}
	if ((img->mono_color &&
	     !eq_cmap(previous_img->in_cmap, previous_img->cmlen,
		      img->in_cmap, img->cmlen)))
	{
	    fprintf( stderr, "%s: Images %s and %s have different colormaps\n",
		    progname, previous_img->title, img->title );
	    return( FATAL_FAILURE );
	}
    }


    /* only dick with the icon if we are not flip_booking */
    if ( !previous_img ) {
	determine_icon_size( img->w, img->h, &img->icn_w, &img->icn_h,
			    &img->icn_factor);

	if ( !img->icn_image )
	    img->icn_image = get_X_image( img, img->icn_w, img->icn_h, False );

	if ( img->icn_image == NULL ) {
	    perror("malloc for fancy icon");
	    return ( MALLOC_FAILURE );
	}
    }

    /*
     * Get image and icon windows of the right size
     */

    create_windows( img, window_geometry );
    set_watch_cursor( img->window );

    /*
     * If we did not inherit the image from the previous image,
     * allocate it now.
     */
    if ( !previous_img || previous_img->pixmap == 0 )
    {
	if (!allocate_ximage( img, True ))
	    return( MALLOC_FAILURE );
    }

    /*
     * Choose a scanline converter...  Here we know the image display makeup,
     * the XImage type and other things...
     */

    if ( !img->map_scanline )
	choose_scanline_converter( img );

    /*
     * If we are going to display a 2 or 3 channel image in one channel (-w)
     * we have to copy the data anyway, so we will always read it into the
     * same spot, and map it into a saved_data array with only one color per
     * scanline this saves memory.
     *
     * read_scan[] is the scan data that we read into with rle_getrow.  If the
     * number of image channels is equal to that which is displayed, we will
     * not mallocate new memory for it, but we will move it throughout the
     * saved_data along with save_scan.
     *
     * save_scan[] is the pointer to the current line in img->saved_data that
     * we are saving rle_getrows into.
     * 
     * If we cant malloc the memory for the save_scan[] or we are doing 
     * flip_book, then we dont want to malloc the memory for this thing.
     */
    
    /*
     * Set up for rle_getrow.  Pretend image x origin is 0. 
     */
    
    /* get img->h rows mem. SAVED_RLE_ROW uses scan_data to calc address!! */
    img->scan_data = NULL;

    /* if we are flip_booking dont mess with this one huh? */
    if ( !previous_img ) {
	img->scan_data =
	    (unsigned char *) malloc ( (img->h + 1) * img->w *
				       (img->dpy_channels) );

	if (img->scan_data == NULL)
	    perror ("malloc for scanline buffer failed");

	/* use the macro to point us to the last line... start saving here */
	save_scan[0] = SAVED_RLE_ROW( img, img->h - 1 );
	for (i = 1; i < img->img_channels; i++)
	    save_scan[i] = save_scan[i - 1] + img->w;
    }

    /* get one line of scan data for reading if we are doing monochrome */
    if ( img->mono_img || img->scan_data == NULL ) {
	read_scan[0] = (unsigned char *) malloc ( img->w * img->img_channels );
	if ( read_scan[0] == NULL ) {
	    perror ("malloc for read_scan buffer failed");
	    return ( MALLOC_FAILURE );
	}
	for (i = 1; i < img->img_channels; i++)
	    read_scan[i] = read_scan[i - 1] + img->w;
    }
    else
	read_scan[0] = save_scan[0],
	read_scan[1] = save_scan[1],
	read_scan[2] = save_scan[2];

    if (img->scan_data == NULL)
	save_scan[0] = read_scan[0],
	save_scan[1] = read_scan[1],
	save_scan[2] = read_scan[2];

    /*
     * For each scan line, read it, save it, dither it and display it. 
     */
    
    view_w = Min(img->w, img->win_w);
    view_h = Min(img->h, img->win_h);
    y_base = view_h - 1;
    img->pan_x = 0;
    img->pan_y = Max(0, img->h - img->win_h );
    img->pan_w = Min(img->w, img->win_w);
    img->pan_h = Min(img->h, img->win_h);
    
    lines_buffered = 0;
    lines_blitted = 0;
    
    while ((image_y = rle_getrow(&img_hdr, read_scan)) <=
	   img_hdr.ymax ) {
	XEvent event;
	
	x11_y = view_h - (image_y - img_hdr.ymin) - 1;
	scan_y = image_ymax - image_y;

	if ( img->mono_img ) 
	    map_rgb_to_bw ( img, read_scan, save_scan[0] );
	else
	    map_rgb_to_rgb ( img, read_scan, save_scan );
	    
	if ( x11_y >= 0 && x11_y < img->h ) {
	    (*img->map_scanline) (img, save_scan, img->dpy_channels,
				  view_w, 1, x11_y, img->image);
	    lines_buffered++;
	}	
	/* Subsample image to create icon */
	
	if ( !previous_img && scan_y % img->icn_factor == 0 ) 
	    (*img->map_scanline)( img, save_scan, img->dpy_channels,
				 img->icn_w, img->icn_factor,
				 scan_y / img->icn_factor, img->icn_image);
	
	while ( XCheckTypedEvent( dpy, Expose, &event ) ) {
	    image_information *eimg;
	    
	    /* get the right window bro....  */
	    for (eimg = img_info; eimg <= img; eimg++ )
		if ( eimg->window == event.xany.window )
		    break;
	    if (previous_img) { /*flip_book override */
		if ( img > img_info && lines_blitted == 0 )
		    eimg = img - 1;
		else
		    eimg = img;
	    }
	    if (eimg <= img) /* protect against screwy window */
		handle_exposure(eimg, event.xexpose.x, event.xexpose.y,
				event.xexpose.width, event.xexpose.height,
				(eimg == img) ? lines_blitted : eimg->h );
	    XFlush(dpy);
	}

	if (lines_buffered >= buffer_scans) {
	    y_base -= lines_buffered - 1;
	    if ( img->pixmap )
		put_X_image(img, img->pixmap, img->gc, img->image,
			    0, y_base, 0, y_base, view_w,
			    lines_buffered);
	    
	    lines_blitted += lines_buffered;
	    handle_exposure(img, img->xo, y_base + img->yo, view_w,
			    lines_buffered, lines_blitted );
			    
	    y_base = x11_y - 1;
	    lines_buffered = 0;
	}

	if ( img->scan_data ) {
	    /* move scan up one line */ 
	    save_scan[0] = SAVED_RLE_ROW ( img, scan_y - 1 );
	    for( i = 1; i < img->img_channels; i++ )
		save_scan[i] = save_scan[i - 1] + img->w;
	}
	/*
	 * remember? if were saving more than one channel then we dont need
	 * to move the data after we read it...  So kludge read_scan ...
	 */
	if ( !img->mono_img && img->scan_data )
	    read_scan[0] = save_scan[0], read_scan[1] = save_scan[1],
	    read_scan[2] = save_scan[2];	
	if ( img->scan_data == NULL)
	    save_scan[0] = read_scan[0], save_scan[1] = read_scan[1],
	    save_scan[2] = read_scan[2];
    }
    
    if ( lines_buffered > 0 ) {
	y_base -= lines_buffered - 1;
	if ( img->pixmap )
	    put_X_image( img, img->pixmap, img->gc, img->image,
			 0, y_base, 0, y_base, view_w,
			 lines_buffered );

	lines_blitted += lines_buffered;
	handle_exposure(img, img->xo, img->yo, view_w,
			lines_buffered, img->h );
			    
    }
    
    if ( !previous_img && img->icn_pixmap ) {
	XPutImage( dpy, img->icn_pixmap, img->icn_gc, img->icn_image,
		  0, 0, 0, 0, img->icn_w, img->icn_h );
	XClearWindow( dpy, img->icn_window );
    }

    set_circle_cursor( img->window );

    /* free this if we mallocated it */
    if ( img->mono_img )
	free ( read_scan[0] );

    if ( previous_img && img->scan_data ) {
	free( img->scan_data );
	img->scan_data = NULL;
    }

    XSync( dpy, False );
    return (SUCCESS);
}

/* 
 * Track events & redraw image when necessary.
 */

/* now thats my kinda action! */
#define ACTION_MAGNIFY		0
#define ACTION_UNMAGNIFY	1
#define ACTION_PAN		2
#define ACTION_SWITCH_MAG_MODE	3
#define ACTION_FLIP_FORWARD	4
#define ACTION_FLIP_STEP	5
#define ACTION_FLIP_BACKWARD	6
#define ACTION_FLIP_SPEED       7
#define ACTION_PIXEL_INFO	8
#define ACTION_CYCLE		9
#define ACTION_CYCLE_TO_AND_FRO 10
#define ACTION_RESIZE 		11
#define ACTION_DEFAULT		ACTION_PAN

/* define what to do on mouse buttons */
static int button_action[3][2] = {
    ACTION_MAGNIFY,	ACTION_PIXEL_INFO,
    ACTION_PAN,		ACTION_SWITCH_MAG_MODE,
    ACTION_UNMAGNIFY,	ACTION_PIXEL_INFO
    };

/* define what to do on flip_book mouse buttons */
static int flip_action[3][2] = {
    ACTION_FLIP_BACKWARD,	ACTION_CYCLE,
    ACTION_FLIP_STEP,		ACTION_FLIP_SPEED,
    ACTION_FLIP_FORWARD,	ACTION_CYCLE_TO_AND_FRO
    };

static
void mag_pan( img, action, bx, by, new_mag_fact )
image_information *img;
int action, bx, by, new_mag_fact;
{
    Boolean fast_pan = False;
    Boolean redraw = False;
    Boolean redither = False;
    int ix = img->pan_x + bx / img->mag_fact;
    int iy = img->pan_y + by / img->mag_fact;
    int opan_x = 0, opan_y = 0;
    int blit_w = 0, blit_h = 0;


    /* perhaps we could re-open the img->filename to do this... */
    /* but NAHHHHHHH */
    if ( img->scan_data == NULL )
	return;

    switch ( action ) {
	/*
	 * Normalize has to switch the current mag factor with 1 if they
	 * differ... It also remembers the old pan_x and pan_y and stuff.  It
	 * should use the pixmaps to refresh, so that it will be fast when
	 * toggeling in this mode...
	 */
    case ACTION_SWITCH_MAG_MODE:
	if ( img->mag_fact == img->save_mag_fact )
	    return;
	else {
	    if ( img->mag_fact == 1 ) {
		img->mag_mode = True;
		img->mag_fact = img->save_mag_fact;

		img->save_pan_x = img->pan_x; img->save_pan_y = img->pan_y;
		img->save_pan_w = img->pan_w; img->save_pan_h = img->pan_h;
		img->save_mag_fact = 1;
	
		img->pan_x = ix - ((img->win_w / 2)/img->mag_fact);
		img->pan_y = iy - ((img->win_h / 2)/img->mag_fact);
		
		img->pan_w = img->win_w/img->mag_fact;
		img->pan_h = img->win_h/img->mag_fact;

	    } else {
#define SWAP(v1, v2) { int tmp = (v1); (v1) = (v2); (v2) = tmp; }
		img->mag_mode = False;

		SWAP( img->save_pan_x, img->pan_x );
		SWAP( img->save_pan_y, img->pan_y );
		SWAP( img->save_pan_w, img->pan_w );
		SWAP( img->save_pan_h, img->pan_h );
#undef SWAP
		img->save_mag_fact = img->mag_fact;

		img->mag_fact = 1;
	    }

	    if (img->save_win_w != img->win_w ||
		img->save_win_h != img->win_h )
	    {
		img->pan_w = img->win_w/img->mag_fact;
		img->pan_h = img->win_h/img->mag_fact;
		
		allocate_ximage( img, False );
		allocate_pixmap( img, False );
	    }
	    
	    img->save_win_w = img->win_w; img->save_win_h = img->win_h;
	    redraw = True;
	    redither = True;
	}
	break;
    case ACTION_MAGNIFY:
    case ACTION_UNMAGNIFY:
	if (img->mag_fact == 1 && new_mag_fact > img->mag_fact)
	{
	    img->save_pan_x = img->pan_x; img->save_pan_y = img->pan_y;
	    img->save_pan_w = img->pan_w; img->save_pan_h = img->pan_h;
	    img->save_win_w = img->win_w; img->save_win_h = img->win_h;
	    img->save_mag_fact = 1;
	}
	
	if (img->mag_fact == 1 && new_mag_fact < img->mag_fact){
	    mag_pan( img, ACTION_PAN, bx, by, img->mag_fact );
	    return;
	}

	img->mag_fact = new_mag_fact;

	if ( img->mag_fact <= 1 ) {
	    img->mag_fact = 1;
	    if ( img->mag_mode )
		redraw = True;
	    img->mag_mode = False;
	} else img->mag_mode = True;
	
	img->pan_x = ix - ((img->win_w / 2)/img->mag_fact);
	img->pan_y = iy - ((img->win_h / 2)/img->mag_fact);

	img->pan_w = img->win_w/img->mag_fact;
	img->pan_h = img->win_h/img->mag_fact;
	
	allocate_ximage( img, False );
	allocate_pixmap( img, False );

	redither = True;
	redraw = True;
	break;

    case ACTION_PAN:
	fast_pan = 1;  /* are we REALLY just panning around? */
	opan_x = img->pan_x;
	opan_y = img->pan_y;
	    
	img->mag_mode = ( img->mag_fact > 1 );
	    
	img->pan_x = ix - ((img->win_w / 2)/img->mag_fact);
	img->pan_y = iy - ((img->win_h / 2)/img->mag_fact);

	    /* Isnt this always like this? */
	img->pan_w = img->win_w/img->mag_fact;
	img->pan_h = img->win_h/img->mag_fact;

	    redither = True;
	    redraw = True;
	break;
    case ACTION_RESIZE:
	img->pan_x = ix - ((img->win_w / 2)/img->mag_fact);
	img->pan_y = iy - ((img->win_h / 2)/img->mag_fact);
		
		/* Isnt this always like this? */
	img->pan_w = img->win_w/img->mag_fact;
	img->pan_h = img->win_h/img->mag_fact;

		redither = True;
		redraw= True;
	break;
    }    

    if (redraw == True)
	calc_view_origin( img );

    /* check bounds */
    if ( img->pan_x < 0 )
	img->pan_x = 0;
    if ( img->pan_y < 0 )
	img->pan_y = 0;
    
    if ( img->pan_w > img->w )
	img->pan_w = img->w;
    if ( img->pan_h > img->h )
	img->pan_h = img->h;
    
    if ( img->pan_x + img->pan_w >= img->w )
    {
	img->pan_x = img->w - img->pan_w;
	if ( img->pan_w * img->mag_fact < img->win_w )
	    img->pan_x -= 1;
    }
    if ( img->pan_y + img->pan_h >= img->h )
    {
	img->pan_y = img->h - img->pan_h;
	if ( img->pan_h * img->mag_fact < img->win_h )
	    img->pan_y -= 1;
    }
    
    /* check bounds */
    if ( img->pan_x < 0 )
	img->pan_x = 0;
    if ( img->pan_y < 0 )
	img->pan_y = 0;

    blit_w = img->w * img->mag_fact;
    blit_h = img->h * img->mag_fact;
	
    blit_w = (blit_w < img->win_w) ? blit_w : img->win_w;
    blit_h = (blit_h < img->win_h) ? blit_h : img->win_h;

    /* let the suckers know that we are thinking */
    set_watch_cursor( img->window );

    /*
     * Some could argue that this fast_pan shit is a waste of time, but it
     * does speed things up a bunch, and its really hard to understand.
     * Sorry, no fancy pictures in the comments.  Just code.  We figure out
     * which rectangle is blt-able.  We blt it on the server side, and on the
     * client side (my fancy XCopyImage) and then MAG_scanline the exposed
     * area and XPutImage that stuff too...  Dont change it cuz its right.
     */
    if ( fast_pan )
    {
	int width, hight;
	int src_x, src_y, dst_x, dst_y;
	int pwidth = opan_x - img->pan_x;
	int phight = opan_y - img->pan_y;
	
	pwidth = img->pan_w - (( pwidth < 0 ) ? - pwidth : pwidth);
	phight = img->pan_h - (( phight < 0 ) ? - phight : phight);

	/*
	 * pwidth and phight now contain the size of the non-changing
	 * (BLT-able) portion of the viewport in rle_pixel space.
	 */
	width = pwidth * img->mag_fact + (blit_w - img->pan_w * img->mag_fact);
	hight = phight * img->mag_fact + (blit_h - img->pan_h * img->mag_fact);

	/* Now we compute the src_xy and dst_xy for the pixel copy */
	if ( opan_x < img->pan_x )
	{
	    dst_x = 0; src_x = blit_w - width;}
	else
	{
	    src_x = 0; dst_x = blit_w - width;}

	if ( opan_y < img->pan_y )
	{
	    dst_y = 0; src_y = blit_h - hight;}
	else
	{
	    src_y = 0; dst_y = blit_h - hight;}

	/* subtract partial pixels if we are going right */
	if ( opan_x < img->pan_x )
	    width -= blit_w - img->pan_w * img->mag_fact;

	/* subtract partial pixels if we are going down */
	if ( opan_y < img->pan_y )
	    hight -= blit_h - img->pan_h * img->mag_fact;

	if ( src_x == dst_x && src_y == dst_y )
	{
	    redraw = action != ACTION_PAN;
	    redither = False;
	}
	else {
	    /* XCopyImage is only implemented for 8 and 32 bit image pixels */
	    if (redither && img->pixmap &&
		XCopyImage(img->image, src_x, src_y, width, hight,
			   dst_x, dst_y))
	    {
		XCopyArea ( dpy, img->pixmap, img->pixmap,
			   img->gc, src_x, src_y, width, hight, dst_x, dst_y );
	    
		if ( dst_y ) {
		    (*img->MAG_scanline)(img, img->pan_x, img->pan_y,
					 img->mag_fact, 0, 0,
					 blit_w, dst_y, img->image );
		    
		    put_X_image( img, img->pixmap, img->gc, img->image,
				 0, 0, 0, 0, blit_w, dst_y);
		}
		else {
		    if (hight < blit_h) {
			(*img->MAG_scanline)
			    (img, img->pan_x, img->pan_y + phight,
			     img->mag_fact, 0, hight,
			     blit_w, blit_h - hight, img->image ); 
			put_X_image(img, img->pixmap, img->gc,
				    img->image, 0, hight, 0, hight,
				    blit_w, blit_h - hight );
		    }
		}
		
		if ( dst_x ) {
		    if (hight && width < img->win_w) {
			(*img->MAG_scanline)
			    (img, img->pan_x, img->pan_y +
			     ((dst_y) ? img->pan_h - phight: 0), img->mag_fact,
			     0, dst_y, blit_w - width, hight, img->image );
		    
			put_X_image(img, img->pixmap, img->gc,img->image,
				    0, dst_y, 0, dst_y, blit_w - width, hight );
		    }
		}
		else {
		    if (hight && width < blit_w) {
			(*img->MAG_scanline)
			    (img, img->pan_x + pwidth, img->pan_y +
			     ((dst_y) ? img->pan_h - phight: 0), img->mag_fact,
			     width, dst_y, blit_w - width, hight, img->image );
		    
			put_X_image(img, img->pixmap, img->gc,
				    img->image, width, dst_y, width, dst_y,
				    blit_w - width, hight );
		    }
		}
		
		/*
		 * We already redithered...  If XCopyImage failed we arent
		 * here and we have to redither the whole thing below.
		 */
		redither = False;
	    }
	}
    }

    /* redither the whole thing */
    if ( redither || ( redraw && !img->pixmap ) )
    {
	(*img->MAG_scanline)(img, img->pan_x, img->pan_y,
			     img->mag_fact, 0, 0, blit_w, blit_h,
			     img->image );

	if (img->pixmap)
	    put_X_image(img, img->pixmap, img->gc, img->image,
			0, 0, 0, 0, blit_w, blit_h );
    }


    if ( redraw )
	handle_exposure(img, 0, 0, img->win_w, img->win_h, img->h);
    set_circle_cursor( img->window );
}

image_information *action_flip_forward( img, img_info, flip_book_udelay, n,
				        mask, event, found_event )
image_information *img, *img_info;
int flip_book_udelay, n;
unsigned long mask;
XEvent *event;
Boolean *found_event;
{
    if (mask) *found_event = False;

    if ( img == &img_info[n - 1] )
	img = &img_info[0];
    for ( ; img < &img_info[n]; img++ ) {
	set_timer( flip_book_udelay );
	handle_exposure(img, 0, 0, img->w, img->h, img->h);
	XFlush( dpy );
	if (mask && XCheckMaskEvent(dpy, mask, event )){
	    *found_event = True;
	    break;
	}
	wait_timer();
    }
    if (mask && *found_event)
	return img;
    else return img - 1;
}

image_information *action_flip_backward( img, img_info, flip_book_udelay, n,
					 mask, event, found_event )
image_information *img, *img_info;
int flip_book_udelay, n;
unsigned long mask;
XEvent *event;
Boolean *found_event;
{
    if (mask) *found_event = False;
    
    if ( img == img_info )
	img = &img_info[n - 1];
    for ( ; img >= &img_info[0]; img-- ) {
	set_timer( flip_book_udelay );
	handle_exposure(img, 0, 0, img->w, img->h, img->h);
	XFlush( dpy );
	if (mask && XCheckMaskEvent(dpy, mask, event )){
	    *found_event = True;
	    break;
	}
	wait_timer();
    }
    if (mask && *found_event)
	return img;
    else return img + 1;
}

image_information *action_flip_book_cycle( img, img_info, n, flip_forward,
					   flip_book_udelay )
image_information *img, *img_info;
int n, flip_book_udelay;
Boolean flip_forward;
{
    int found_event;
    XEvent event;
    
    do {
	img = (* (flip_forward ? action_flip_forward : action_flip_backward))
	    (img, img_info, flip_book_udelay, n, ButtonPressMask|KeyPressMask,
	     &event, &found_event);
	XSync(dpy, False);
    } while ( !found_event );    
    return img;
}

/* returns whether or not we should redither and redraw */
int
resize_window( img, new_w, new_h )
image_information *img;
int new_w, new_h;
{
    int old_w = img->win_w;
    int old_h = img->win_h;
    int iw = img->w * img->mag_fact;
    int ih = img->h * img->mag_fact;
    int retval = 0;
    
    img->win_w = new_w;
    img->win_h = new_h;
    calc_view_origin( img );

    if ( old_w < iw || new_w < iw || old_h < ih || new_h < ih ) { 
	allocate_ximage( img, True );
	if (img->pixmap)
	    allocate_pixmap( img, True );
	retval = 1;
    }
    
    return retval;
}

static void
update_pic( img_info, n, flip_book, flip_book_frams_sec )
image_information *img_info;
int n, flip_book_frams_sec;
Boolean flip_book;
{
    int i;
    XEvent event;
    int action;
    register image_information *img;
    int found_event;
    
    /* variables to use for flip_book mode */
    image_information *flip_frame = NULL;
    Boolean flip_forward = True;
    Window dead_window;
    int flip_book_udelay = 0;
    
    if ( flip_book ) {
	img = flip_frame = img_info;
	handle_exposure(img, 0, 0, img->w, img->h, img->h);
	XStoreName(dpy, img->window, img->title);
    }

    while (n) {
	
        XNextEvent(dpy, &event);

	if ( flip_book ){  /* all windows are the same! */
	    flip_book_udelay =
		(flip_book_frams_sec) ? 1000000 / flip_book_frams_sec : 0;
	    img = flip_frame;
	} else
	    for ( img = img_info; img < &img_info[n]; img++ )
		if ( img->window == event.xany.window )
		    break;
		
	switch (event.type) {
	    
	case ButtonPress:
	    i = event.xbutton.button - Button1;
	    if (i < 0 || i > COUNT_OF(button_action) )
		action = ACTION_DEFAULT;
	    else
		if ( flip_book )
		    action = flip_action[i][event.xbutton.state & ShiftMask];
		else
		    action = button_action[i][event.xbutton.state & ShiftMask];
	    
	    switch (action) {
		
	    case ACTION_PIXEL_INFO:
		if ( img->scan_data ) do
		{/* X calls by John Bradley, U of Penn, hacked up by mrf.... */
		    Boolean first = 1;
		    int lx = 0, ly = 0;
		    int xo = Max(0, (img->win_w - (img->w * img->mag_fact))/2);
		    int yo = Max(0, (img->win_h - (img->h * img->mag_fact))/2);

		    set_left_ptr_cursor( img->window );
		    MapPixWindow ( img, (event.xbutton.y > img->win_h/2));
		    while (1) {     /* loop until button released */
			Window foo,poo;
			int rx, ry, x, y;
			unsigned int mask;
			
			if (XQueryPointer(dpy, img->window, &foo, &poo,
					  &rx, &ry, &x, &y, &mask))
			{
			    if (!(mask&(Button1Mask|Button2Mask|Button3Mask)))
				break; /* released */
			    
			    x -= xo;		y -= yo;
			    x /= img->mag_fact;	y /= img->mag_fact;
			    x += img->pan_x;	y += img->pan_y;
				
				/* wait for new pixel */
				if ((first || x != lx || y != ly) && 
				    (x >= 0 && x < img->w &&
				     y >= 0 && y < img->h)) {
				    DrawPixWindow( img, x, y );
				    first = 0;  lx = x;  ly = y;
				}
			} else
			    break;
		    }
		    set_circle_cursor( img->window );
		    UnmapPixWindow ( img );
		} while (0); 
		continue;

	    case ACTION_FLIP_SPEED:
		if (flip_book) do
		{/* X calls by John Bradley, U of Penn, hacked up by mrf.... */
		    Boolean first = 1, ly = 0;
		    int height = DisplayHeight(dpy, screen)/4;  /* use DH/4 */
		    int s;

		    s = flip_book_frams_sec;
		    MapPixWindow ( img, True );

		    while (1) {     /* loop until button released */
			Window foo,poo;
			int rx, ry, x, y, inc;
			unsigned int mask;
			
			if (XQueryPointer(dpy, img->window, &foo, &poo,
					  &rx, &ry, &x, &y, &mask)) {
			    if (!(mask&(Button1Mask|Button2Mask|Button3Mask)))
				break; /* released */
			    
			    if ( first )
				ly = y;
			    inc = (ly - y) * 100 / height;
			    
			    /* wait for new pixel */
			    if ((first || flip_book_frams_sec + inc != s ) ) {
				s = flip_book_frams_sec + inc;
				s = (s < 0) ? 0 : s;
				s = (s > 100) ? 100 : s;
				DrawSpeedWindow( img, s );
				first = 0; 
			    }
			}
			else
			    break;
		    }
		    flip_book_frams_sec = s;
		    UnmapPixWindow ( img );
		} while (0); 
		continue;

	    case ACTION_FLIP_BACKWARD:
		if ( flip_book ) {
		    img = action_flip_backward(img, img_info, flip_book_udelay,
					       n, ButtonPressMask|KeyPressMask,
					       &event, &found_event);
		    flip_frame = img;
		    /* Restore title after flipping has stopped. */
		    XStoreName(dpy, img->window, img->title);
		    flip_forward = False;
		}
		continue;
		
	    case ACTION_FLIP_FORWARD:
		if ( flip_book ) { 
		    img = action_flip_forward (img, img_info, flip_book_udelay,
					       n, ButtonPressMask|KeyPressMask,
					       &event, &found_event);
		    flip_frame = img;
		    /* Restore title after flipping has stopped. */
		    XStoreName(dpy, img->window, img->title);
		    flip_forward = True;
		}
		continue;

          case ACTION_FLIP_STEP:
              /* step flip_book in current direction. */
              if ( flip_book ) {
                  if ( flip_forward ) {
                      if ( flip_frame == &img_info[n-1] )
                          img = flip_frame = img_info;
                      else
                          img = flip_frame = flip_frame + 1;
                  }
                  else {
                      if ( flip_frame == img_info )
                          img = flip_frame = &img_info[n-1];
                      else
                          img = flip_frame = flip_frame - 1;
                  }
                  handle_exposure(img, 0, 0, img->w, img->h, img->h);
                  XStoreName(dpy, img->window, img->title);
              }
              continue;

		/* cycle in the current (flip_forward) direction */
	    case ACTION_CYCLE:
		if ( flip_book ) {
		    flip_frame = img = action_flip_book_cycle
			( img, img_info, n, flip_forward, flip_book_udelay );
		    /* Restore title after flipping has stopped. */
		    XStoreName(dpy, img->window, img->title);
              }
              continue;
          case ACTION_CYCLE_TO_AND_FRO:
              if ( flip_book ) {
                  do {
                      img = (* (flip_forward ?
                                action_flip_forward :
                                action_flip_backward))
                          (img, img_info, flip_book_udelay,
                           n, ButtonPressMask |KeyPressMask,
                           &event, &found_event);
                      if (!found_event){
                          flip_forward = !flip_forward;
                          if ( n > 1 )
                              img = (flip_forward) ? img + 1 : img - 1;
                      }
                      XSync(dpy, False);
                  } while( !found_event );
                  flip_frame = img;
		  /* Restore title after flipping has stopped. */
		  XStoreName(dpy, img->window, img->title);
	      }
		continue;

	    case ACTION_MAGNIFY:
		mag_pan ( img, action, event.xbutton.x, event.xbutton.y,
			  img->mag_fact + 1 );
		continue;
	    case ACTION_UNMAGNIFY:
		mag_pan ( img, action, event.xbutton.x, event.xbutton.y,
			  img->mag_fact - 1 );
		continue;
	    case ACTION_SWITCH_MAG_MODE:
		mag_pan ( img, action, event.xbutton.x, event.xbutton.y,
			  img->mag_fact );
		continue;

	    case ACTION_PAN:
		mag_pan ( img, action, event.xbutton.x, event.xbutton.y,
			  img->mag_fact );
		continue;

	    default:
		continue;
	    }
	    
	    break; /* not reached */
	    
	case Expose:
	    handle_exposure(img, event.xexpose.x, event.xexpose.y,
			    event.xexpose.width, event.xexpose.height,
			    img->h );

	    continue;

	case ConfigureNotify:
	{
	    int new_w = event.xconfigure.width;
	    int new_h = event.xconfigure.height;
	    
	    if ( !flip_book && (img->win_w != new_w || img->win_h != new_h )) {
		if (resize_window( img, new_w, new_h ))
		    mag_pan (img, ACTION_RESIZE, img->win_w/2, img->win_h/2,
			     img->mag_fact );
	    }
	}
	    continue;

	case NoExpose:
	    continue;
	    
	case KeyPress:
	{
	    char string[256];
	    char *symstr, *XKeysymToString( );
	    KeySym keysym;
	    int length;
	    XComposeStatus stat;
	    Boolean handled_key = True;
	    Boolean shifted_key;
	  
	    length = XLookupString( (XKeyEvent *)&event, string, 256, &keysym, &stat );
	    string [length] = '\0';
	    symstr = XKeysymToString( keysym ); 
	    shifted_key = event.xkey.state & ShiftMask;
	    
	    if ( length == 1 && (string[0] == 'q' || string[0] == 'Q' ||
				 string[0] == '\003' )) /* q, Q or ^C */
		break;

	    if ( length == 1 )
		switch (string[0])
		{
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8':
		case '9':
		    mag_pan ( img, ACTION_MAGNIFY, img->win_w/2, img->win_h/2,
			     atoi(string));
		    break;

		    /* back and forth mode */
		case 'b':
		    if ( flip_book ) {
			do {
			    img = (* (flip_forward ?
				      action_flip_forward :
				      action_flip_backward))
				(img, img_info, flip_book_udelay,
				 n, ButtonPressMask |KeyPressMask,
				 &event, &found_event);
			    if (!found_event){
				flip_forward = !flip_forward;
				if ( n > 1 )
				    img = (flip_forward) ? img + 1 : img - 1;
			    }
			    XSync(dpy, False);
			} while( !found_event );
			flip_frame = img;
		    }
		    break;

		case 'c':
		case 'C':
		    if ( flip_book ) {
			flip_forward = (string[0] == 'c');
			flip_frame = img = action_flip_book_cycle
			    (img, img_info, n, flip_forward, flip_book_udelay);
		    }
		    break;

		case 's':
		    XDrawString ( dpy, img->window, img->gc,
				 4, 20, "hi there", strlen ("hi there") );

		    break;

		case 'r':
		case 'R':
		    /* remove a frame from flip book */
		    if (flip_book ) {
			n--;
			if ( n )   /* pack imgs in there good */  
			    for (img = flip_frame; img < &img_info[n]; img++ )
				*img = *(img+1);
			else exit(0);
			if ( flip_forward ) {
			    if ( flip_frame == &img_info[n] )
				img = flip_frame = img_info;
			    else
				img = flip_frame;
			}
			else {
			    if ( flip_frame == img_info )
				img = flip_frame = &img_info[n-1];
			    else
				img = flip_frame = flip_frame - 1;
			}
			handle_exposure(img, 0, 0, img->w, img->h, img->h);
			XStoreName(dpy, img->window, img->title);
		    }
		    break;
		case ' ':
		    /* step flip_book in current direction. */
		    if ( flip_book ) {
			if ( flip_forward ) {
			    if ( flip_frame == &img_info[n-1] )
				img = flip_frame = img_info;
			    else
				img = flip_frame = flip_frame + 1;
			}
			else {
			    if ( flip_frame == img_info )
				img = flip_frame = &img_info[n-1];
			    else
				img = flip_frame = flip_frame - 1;
			}
			handle_exposure(img, 0, 0, img->w, img->h, img->h);
			XStoreName(dpy, img->window, img->title);
		    }
		    break;

		case '\010':
		case '\177':
		    /* step flip_book opposite to current direction. */
		    if ( flip_book ) {
			if ( !flip_forward ) {
			    if ( flip_frame == &img_info[n-1] )
				img = flip_frame = img_info;
			    else
				img = flip_frame = flip_frame + 1;
			}
			else {
			    if ( flip_frame == img_info )
				img = flip_frame = &img_info[n-1];
			    else
				img = flip_frame = flip_frame - 1;
			}
			handle_exposure(img, 0, 0, img->w, img->h, img->h);
			XStoreName(dpy, img->window, img->title);
		    }
		    break;

		case 'i':
		case 'I':
		    /* Install/deinstall colormap.
		     * Should only do this if no window manager
		     * running, but that's hard to tell.  Let user
		     * deal with it...
		     */
		    if ( img->colormap )
			if ( string[0] == 'i' )
			    XInstallColormap( dpy, img->colormap );
			else
			    XUninstallColormap( dpy, img->colormap );
		    break;

		default:
		    handled_key = False;
		}
	    else handled_key = False;
		
	    DPRINTF(stderr, "%s %zx, %s String '%s' - %d\n", symstr, keysym,
		    ( shifted_key )? "shifted":"unshifted", string, length );

	    if ( !handled_key )
	    {
		/* GACK! the F28-34 keysyms are for the suns!              */
		/* on the DECs they are Left Right Up and Down w/ShiftMask */
		/* on the ardent they are KP_4 KP_6 KP_8 KP_2 w/ShiftMask  */
		/* insert your favorite shifted arrow keysyms here!        */
		
		if ( !strcmp( symstr, "Left" ) || !strcmp( symstr, "F30" ) )
		    mag_pan( img, ACTION_PAN, (shifted_key ? 0 : img->win_w/4),
			     img->win_h/2, img->mag_fact);
		if ( !strcmp( symstr, "Up" ) || !strcmp( symstr, "F28" ) )
		    mag_pan( img, ACTION_PAN, img->win_w/2,
			    (shifted_key ? 0 : img->win_h/4), img->mag_fact);
		if ( !strcmp( symstr, "Right" ) || !strcmp( symstr, "F32" ) )
		    mag_pan( img, ACTION_PAN,
			     (shifted_key ? img->win_w-1 :
			      img->win_w/2 + img->win_w/4),
			     img->win_h/2, img->mag_fact);
		if ( !strcmp( symstr, "Down" ) || !strcmp( symstr, "F34" ) )
		    mag_pan( img, ACTION_PAN, img->w/2,
			     (shifted_key ? img->win_h-1 :
			      img->win_h/2 + img->win_h/4),
			     img->mag_fact);
	    }
	    continue;
	}

	case VisibilityNotify:
	case CreateNotify:
	case DestroyNotify:
	case UnmapNotify:
	case MapNotify:
	case MapRequest:
	case ReparentNotify:
	case ConfigureRequest:
	    continue;

	case MappingNotify:
	    XRefreshKeyboardMapping( &event.xmapping );
	    continue;

	default: 
	    fprintf(stderr, "%s: Event type %x?\n", progname, event.type);
	    continue;

	}

	/* exit this window */
	
	if (img->scan_data)
	    free ( img->scan_data );
	if (img->icn_image)
	    destroy_X_image( img, img->icn_image );
	if (img->image)
	    destroy_X_image( img, img->image );
	if (img->pixmap)
	    free_X_pixmap( img, img->pixmap );
	if (img->colormap != DefaultColormap( dpy, screen ))
	    XFreeColormap( dpy, img->colormap );
	else
	    free_image_colors( img );
	if (img->icn_pixmap)
	    free_X_pixmap( img, img->icn_pixmap );
	if (img->icn_gc)
	    XFreeGC( dpy, img->icn_gc );
	if (img->gc)
	    XFreeGC( dpy, img->gc );
	if (img->icn_window)
	    XDestroyWindow ( dpy, img->icn_window );

	dead_window = img->window;
	if (img->window)
	    XDestroyWindow ( dpy, img->window );
	
	n--;
	if ( n )   /* pack imgs in there good */  
	    for ( ; img < &img_info[n]; img++ )
		*img = *(img+1);
	else break;

	/* flipbook only has one window, so if its dead_window then die too */
	if ( img_info[0].window == dead_window )
	{
#ifdef X_SHARED_MEMORY
	    for ( img = &img_info[0] ; img < &img_info[n]; img++ )
	    {
		if ( img->shm_pix.shmid >= 0 )
		    free_X_pixmap( img, img->pixmap );
		if ( img->shm_img.shmid >= 0 && img->image )
		    destroy_X_image( img, img->image );
	    }
#endif
	    break;
	}
    }
}
