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
 * x11_stuff.c - Do colormaps, visuals, pix_info window...
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
#include "getx11.h"

#ifdef X_SHARED_MEMORY
#include <sys/errno.h>
#endif
#include <errno.h>

#include "circle.bitmap"
#include "circle_mask.bitmap"

static Boolean init_separate_color_rw(), init_color_rw(),
    init_separate_color_ro(), init_color_ro(), init_mono_rw(), init_mono_ro();
static CONST_DECL char *visual_class_to_string();

static Cursor circle_cursor = 0;
static Cursor left_ptr_cursor = 0;
static Cursor watch_cursor = 0;

static int specified_screen = -1;	/* No specific screen. */

static image_information *wait_img = NULL;

void set_watch_cursor( window )
Window window;
{
    XDefineCursor( dpy, window, watch_cursor );
    XFlush(dpy);
}

void set_circle_cursor( window )
Window window;
{
    XDefineCursor( dpy, window, circle_cursor );
    XFlush(dpy);
}

void set_left_ptr_cursor( window )
Window window;
{
    XDefineCursor( dpy, window, left_ptr_cursor );
    XFlush(dpy);
}

void get_cursors( window )
Window window;
{
    if (circle_cursor == 0)
	circle_cursor = XCreateFontCursor (dpy, XC_circle);

    if (watch_cursor == 0)
	watch_cursor = XCreateFontCursor (dpy, XC_watch);

    if (left_ptr_cursor == 0)
	left_ptr_cursor = XCreateFontCursor (dpy, XC_left_ptr);

    if (circle_cursor == 0) {
	Pixmap	source;
	Pixmap	mask;
	XColor	color_1;
	XColor	color_2;

	source = XCreateBitmapFromData(dpy, window, circle_bits,
				       circle_width, circle_height);

	mask = XCreateBitmapFromData(dpy, window, circle_mask_bits,
				     circle_width, circle_height);

	color_1.pixel = WhitePixel (dpy, screen);
	color_1.red   = 0xffff;
	color_1.green = 0xffff;
	color_1.blue  = 0xffff;
	color_1.flags = DoRed | DoGreen | DoBlue;

	color_1.pixel = BlackPixel (dpy, screen);
	color_2.red   = 0;
	color_2.green = 0;
	color_2.blue  = 0;
	color_2.flags = DoRed | DoGreen | DoBlue;

	circle_cursor = XCreatePixmapCursor (dpy, source, mask,
					     &color_1, &color_2,
					     circle_x_hot, circle_y_hot);
	XFreePixmap (dpy, source);
	XFreePixmap (dpy, mask);
    }

    if (watch_cursor == 0)
	watch_cursor = circle_cursor;

    if (left_ptr_cursor == 0)
	left_ptr_cursor = circle_cursor;
}


XImage *get_X_image( img, width, height, share )
image_information *img;
int width, height;
Boolean share;
{
    XImage 		*image;
#ifdef X_SHARED_MEMORY
    int			shmid = -1;

    if ( use_shared_pixmaps && share )
    {
	img->shm_img.shmid = -1;
	image = XShmCreateImage( dpy, img->dpy_visual,
				 (img->binary_img ? 1 : img->dpy_depth), 
				 (img->binary_img ? XYBitmap : ZPixmap), 
				 NULL, &img->shm_img, width, height );
	if ( image != 0 )
	{
	    /* Allocate shared segment. */
	    img->shm_img.shmid =
		shmget( IPC_PRIVATE,
			image->bytes_per_line * image->height,
			IPC_CREAT|0777 );
	    if ( img->shm_img.shmid < 0 )
	    {
		if ( errno == ENOSPC )
		{
		    if ( !no_shared_space )
			fprintf( stderr,
				 "getx11: No more shared memory segments\n" );
		    no_shared_space = True;
		}
		else
		    perror( "getx11: shared memory allocation failed" );
		XDestroyImage( image );
		image = NULL;
	    }
	    else
	    {
		/* Attach the segment & get its address. */
		img->shm_img.shmaddr = image->data =
		    shmat( img->shm_img.shmid, 0, 0 );

		if ( image->data == NULL) {
		    perror ( "getx11: attach shared image->data" );
		    XDestroyImage (image);
		    shmctl( img->shm_img.shmid, IPC_RMID, 0 );
		    img->shm_img.shmid = -1;
		    image = NULL;
		}
		else
		{
		    /* Want to write into the segment! */
		    img->shm_img.readOnly = False;

		    /* Tell the X server about it. */
		    if ( !XShmAttach( dpy, &img->shm_img ) )
		    {
			fprintf( stderr, "getx11: XShmAttach failed.\n" );
			XDestroyImage (image);
			shmdt( img->shm_img.shmaddr );
			shmctl( img->shm_img.shmid, IPC_RMID, 0 );
			img->shm_img.shmid = -1;
			image = NULL;
		    }
		}
	    }
	}
	shmid = img->shm_img.shmid;
    }

    /* If not sharing, or if sharing failed, make a normal XImage. */
    if ( shmid >= 0 )
    {
	/* Remove it now so it will go when we die.
	 * Another nice side effect: we don't fill up the SHM table,
	 * so we can allocate hundreds of them.
	 */
	XSync( dpy, False );	/* Make sure the server knows about it. */
	shmctl( shmid, IPC_RMID, 0 );
    }
    else
#endif
    {
	image = XCreateImage ( dpy, img->dpy_visual,
			       (img->binary_img ? 1 : img->dpy_depth), 
			       (img->binary_img ? XYBitmap : ZPixmap), 
			       0, NULL, width, height, 32, 0);

	if (image != NULL) {
	    image->data = (char *)malloc ( image->bytes_per_line * height );
	    if ( image->data == NULL) {
		perror ( "malloc image->data" );
		XDestroyImage (image);
		image = NULL;
	    }
	}
    }
    return (image);

}

void
destroy_X_image( img, image )
image_information *img;
XImage *image;
{ 
#ifdef X_SHARED_MEMORY
    if ( use_shared_pixmaps && image == img->image && img->shm_img.shmid >= 0 )
    {
	XShmDetach (dpy, &img->shm_img);
	XDestroyImage (image);
	shmdt( img->shm_img.shmaddr );
	shmctl( img->shm_img.shmid, IPC_RMID, 0 );
	img->shm_img.shmid = -1;
    }
    else
#endif
	XDestroyImage( image );

    image = NULL;
}

static int handle_x_errors( dpy, event)
	Display *dpy;
	XErrorEvent *event;
{
    XID xid = event->resourceid;
    image_information *img = wait_img;

    switch ( event->error_code ) {
    case BadAlloc:
	if ( img ) {
	    if ( xid == img->pixmap ){
		DPRINTF(stderr, "img->pixmap allocation failed\n");
		img->pixmap = 0;
		img->pixmap_failed = True;
	    } else
	    if ( xid == img->icn_pixmap ){
		DPRINTF(stderr, "img->icn_pixmap allocation failed\n");
		img->icn_pixmap = 0;
		img->pixmap_failed = True;
	    } else
		_XDefaultError (dpy, event);
	    break;
	}
    case BadWindow:
    case BadDrawable:
    case BadMatch:
	_XDefaultError( dpy, event );
	break;
    default:
	_XDefaultError( dpy, event );
	break;
    }
    return 0;
}

void check_pixmap_allocation( img )
image_information *img;
{
    wait_img = img;
    XSetErrorHandler( handle_x_errors );
    XSync( dpy, False );
}

int allocate_ximage( img, reallocate )
image_information *img;
Boolean reallocate;
{
    int iw = img->w * img->mag_fact;
    int ih = img->h * img->mag_fact;

    if ( img->image != NULL &&
	( reallocate || (img->image->width < iw || img->image->height < ih))) {
	destroy_X_image( img, img->image );
	img->image = NULL;
    }
    if (img->image == NULL) {
	int w, h;
	    w = Min( iw, img->win_w );
	    h = Min( ih, img->win_h );
	img->image = get_X_image( img, w, h, True ); 
    }
    if ( img->image == NULL )
	perror("problem getting XImage");

    return( img->image != NULL );
}

void allocate_pixmap( img, reallocate )
image_information *img;
Boolean reallocate;
{
    extern int stingy_flag;

    int iw = img->w * img->mag_fact;
    int ih = img->h * img->mag_fact;

    if (( img->pixmap != 0 && reallocate ) ||
	( img->pixmap != 0 && (img->pix_w < iw || img->pix_h < ih ))){
	XFreePixmap( dpy, img->pixmap );
	img->pixmap = 0;
    }

    /* reallocate it: use the Min of the winsize and (pic * mag) */
    if ( !img->pixmap && !img->pixmap_failed && !stingy_flag) {
	img->pix_w = Min( iw, img->win_w );
	img->pix_h = Min( ih, img->win_h );
#ifdef X_SHARED_MEMORY
	img->shm_pix.shmid = -1;
	if ( use_shared_pixmaps )
	{
	    XImage *image;
	    XShmSegmentInfo shminfo;

	    /* Allocate an image structure to get bytes_per_line. */
	    image = XShmCreateImage( dpy, img->dpy_visual,
				     img->dpy_depth, ZPixmap, 
				     NULL, &shminfo,
				     img->pix_w, img->pix_h );
	    /* Get a shared segment for the pixmap. */
	    img->shm_pix.shmid =
		shmget( IPC_PRIVATE,
			image->bytes_per_line * image->height,
			IPC_CREAT|0777 );
	    XDestroyImage( image );
	    if ( img->shm_pix.shmid < 0 )
	    {
		if ( errno == ENOSPC )
		{
		    if ( !no_shared_space )
			fprintf( stderr,
				 "getx11: No more shared memory segments\n" );
		    no_shared_space = True;
		}
		else
		    perror( "getx11: shared memory allocation failed" );
	    }
	    else
	    {
		/* Attach the segment & get its address. */
		img->shm_pix.shmaddr = shmat( img->shm_pix.shmid, 0, 0 );

		if ( img->shm_pix.shmaddr == NULL) {
		    perror ( "getx11: attach shared pixmap" );
		    shmctl( img->shm_pix.shmid, IPC_RMID, 0 );
		    img->shm_pix.shmid = -1;
		}
		else
		{
		    /* Want to write into the segment! */
		    img->shm_pix.readOnly = False;

		    /* Tell the X server about it. */
		    if ( !XShmAttach( dpy, &img->shm_pix ) )
		    {
			fprintf( stderr, "getx11: XShmAttach failed.\n" );
			shmdt( img->shm_pix.shmaddr );
			shmctl( img->shm_pix.shmid, IPC_RMID, 0 );
			img->shm_pix.shmid = -1;
		    }
		    else
		    {
			img->pixmap = XShmCreatePixmap(
			    dpy, img->window, img->shm_pix.shmaddr,
			    &img->shm_pix, img->pix_w, img->pix_h,
			    img->dpy_depth );
		    }
		}
	    }
	}

	/* If not sharing, or sharing failed, then try to do it normally. */
	if ( img->shm_pix.shmid < 0 )
#endif /* X_SHARED_MEMORY */
	    img->pixmap = XCreatePixmap(dpy, img->window,
					img->pix_w, img->pix_h,
					img->dpy_depth );
	check_pixmap_allocation( img );
#ifdef X_SHARED_MEMORY
	if ( img->shm_pix.shmid >= 0 )
	{
	    /* Remove it now so it will go when we die.
	     * Another nice side effect: we don't fill up the SHM table,
	     * so we can allocate hundreds of them.
	     */
	    shmctl( img->shm_pix.shmid, IPC_RMID, 0 );
	}
#endif
    }
}

void
free_X_pixmap( img, pix )
image_information *img;
Pixmap pix;
{
    XFreePixmap( dpy, pix );
#ifdef X_SHARED_MEMORY
    if ( use_shared_pixmaps && pix == img->pixmap && img->shm_pix.shmid >= 0 )
    {
	XShmDetach( dpy, &img->shm_pix );
	shmdt( img->shm_pix.shmaddr );
	shmctl( img->shm_pix.shmid, IPC_RMID, 0 );
	img->shm_pix.shmid = -1;
    }
#endif
}

void
put_X_image( img, d, gc, image, src_x, src_y, dest_x, dest_y,
	     width, height )
image_information *img;
Drawable d;
GC gc;
XImage *image;
int src_x, src_y, dest_x, dest_y;
unsigned int width, height;
{
#ifdef X_SHARED_MEMORY
    if ( use_shared_pixmaps && img->image == image && img->shm_img.shmid >= 0 )
	XShmPutImage( dpy, d, gc, image, src_x, src_y, dest_x, dest_y,
		      width, height, False );
    else
#endif
	XPutImage( dpy, d, gc, image, src_x, src_y, dest_x, dest_y,
		   width, height );
}

void
determine_icon_size(image_width, image_height,
		    icon_width, icon_height, icon_factor )
int image_width, image_height;
int *icon_width, *icon_height, *icon_factor;
{
    XIconSize	*icon_sizes;
    int	icon_size_count;
    int width, height, factor;
    int i;

    /*
     * We want the Icon to be about DESIRED_ICON_WIDTH x DESIRED_ICON_HEIGHT.  
     * First, make sure that is OK with the window manager.
     * Then figure out the icon scaling factor.
     */

#define DESIRED_ICON_WIDTH	48
#define DESIRED_ICON_HEIGHT	48

    width = DESIRED_ICON_WIDTH;
    height = DESIRED_ICON_HEIGHT;

    if (XGetIconSizes (dpy, root_window, &icon_sizes, &icon_size_count) 
	&& icon_size_count >= 1) {

	for (i = 0; i < icon_size_count; i++) {
	    if (icon_sizes[i].min_width <= DESIRED_ICON_WIDTH
		&& icon_sizes[i].max_width >= DESIRED_ICON_WIDTH
		&& icon_sizes[i].min_height <= DESIRED_ICON_HEIGHT
		&& icon_sizes[i].max_height >= DESIRED_ICON_HEIGHT) {
		break;
	    }
	}

	if (i >= icon_size_count) {
	    width = icon_sizes[0].max_width;
	    height = icon_sizes[0].max_height;
	}

    }

    factor = image_width / width;
    if (factor < image_height / height) 
	factor = image_height / height;
    if ( factor == 0 )
	factor = 1;

    *icon_width = 1 + (image_width / factor);
    *icon_height = 1 + (image_height / factor);
    *icon_factor = factor;
}


void get_x_colormap( img )
image_information *img;
{
    img->colormap = XCreateColormap (dpy, root_window,
				     img->dpy_visual, AllocNone );
    if (img->colormap == 0) 
	fprintf(stderr, "getx11: Could not create color map for visual\n");
    else {
	VPRINTF(stderr, "created colormap for visual type %s\n",
		visual_class_to_string(img->visual_class));
    }
}

void open_x_display( display_name )
char *display_name;
{
    /* Check to see if user explicitly specified the screen with -d. */
    if ( display_name )
    {
	char *dotp = strrchr( display_name, '.' );
	char *colonp = strrchr( display_name, ':' );

	/* If the dot follows the colon, then it's a screen designator. */
	if ( dotp && colonp && dotp > colonp && dotp[1] != '\0' )
	    specified_screen = atoi( dotp+1 );
    }

    if (display_name == NULL || *display_name == '\0')
	display_name = getenv("DISPLAY");

    dpy = XOpenDisplay(display_name);

    if (dpy == NULL) {
	fprintf(stderr, "%s: Cant open display %s\n", progname,
		(display_name == NULL) ? "" : display_name);
	exit(1);
    }

#ifdef X_SHARED_MEMORY
    if (do_sharing)
    {
	Status sh_status;
	int sh_major, sh_minor;
	Boolean sh_pixmaps = False;

	sh_status = XShmQueryVersion( dpy, &sh_major, &sh_minor, &sh_pixmaps );
	use_shared_pixmaps = (Boolean)(sh_status && sh_pixmaps);
    }
#endif
    /* Work around bug in X11/NeWS server colormap allocation. */
    if (strcmp("X11/NeWS - Sun Microsystems Inc.", ServerVendor(dpy)) == 0 &&
	VendorRelease(dpy) == 2000)
	no_color_ref_counts = True;

}

void calc_view_origin( img )
image_information *img;
{
    img->xo = (img->win_w - (img->w * img->mag_fact)) / 2;
    img->yo = (img->win_h - (img->h * img->mag_fact)) / 2;
    img->xo = Max (0, img->xo);
    img->yo = Max (0, img->yo);
}
/*
 * Create a window with no help from user.
 */
void
create_windows( img, window_geometry )
char			*window_geometry;
register image_information *img;
{
    static char prog_name[] = "getx11";
    static char class_name[] = "Getx11";
    static XClassHint	class_hint = {prog_name, class_name};
    char		default_geometry[30];
    int			width, height, x, y;
    XSizeHints		size_hints;
    unsigned int	mask;
    unsigned long	gc_mask = 0;
    XGCValues		gc_values;
    XWMHints		wm_hints;
    XSetWindowAttributes xswa;
    unsigned long	xswa_mask;
    Boolean 		new_window;
    int icn_alloc = False;
    Boolean		new_pixmaps;

    /*
     * Now, make the window.
     */

    new_window = (img->window == 0 && img->icn_window == 0);
    new_pixmaps = ((img->pixmap == 0 || img->icn_pixmap == 0) &&
		   !img->pixmap_failed && !stingy_flag);

    if ( !img->window )
    {
	sprintf( default_geometry, "=%dx%d", img->w, img->h );

	mask = XGeometry(dpy, screen, window_geometry, default_geometry, 
			 IMAGE_BORDERWIDTH, 1, 1, 0, 0,
			 &x, &y, &width, &height );

	size_hints.flags = 0;

	if ( mask & (XValue | YValue) ) {
	    size_hints.flags |= USPosition;
	    size_hints.x = x;
	    size_hints.y = y;
	} else {
	    size_hints.flags |= PPosition;
	    size_hints.x = x = (DisplayWidth(dpy, screen) - width)/2;
	    size_hints.y = y = (DisplayHeight(dpy, screen) - height)/2;
	}

	if ( mask & (WidthValue | HeightValue) ) {
	    size_hints.flags |=	USSize;
	    size_hints.width = width;
	    size_hints.height = height;
	}

	VPRINTF (stderr, "window at (%d, %d) %dx%d\n", 
		 x, y, width, height);

	xswa_mask = CWBackPixel | CWEventMask | CWBorderPixel;
	xswa.background_pixel = BlackPixel (dpy, screen);
	xswa.border_pixel = WhitePixel (dpy, screen);
	xswa.event_mask = ButtonPressMask | ExposureMask | KeyPressMask |
	    StructureNotifyMask;

	if (img->colormap) {
	    xswa.colormap = img->colormap;
	    xswa_mask |= CWColormap;
	}

	img->window = XCreateWindow ( dpy, root_window, x, y, 
				      width, height, 
				      IMAGE_BORDERWIDTH, img->dpy_depth,
				      InputOutput, img->dpy_visual,
				      xswa_mask, &xswa);

	img->win_w = width;
	img->win_h = height;
	calc_view_origin ( img );

	XSetNormalHints (dpy, img->window, &size_hints);
	XSetClassHint(dpy, img->window, &class_hint);
	XSetIconName (dpy, img->window, img->title);
    }
    XStoreName(dpy, img->window, img->title);

    if ( new_pixmaps )
	allocate_pixmap( img, False );

    if ( !img->icn_window )
    {
	/* dies with BadMatch on some server/visual pairs without these?!?!? */
	xswa_mask = CWBackPixel | CWBorderPixel;
	xswa.background_pixel = BlackPixel (dpy, screen);
	xswa.border_pixel = WhitePixel (dpy, screen);

	if (img->colormap) {
	    xswa.colormap = img->colormap;
	    xswa_mask |= CWColormap;
	}

	img->icn_window = XCreateWindow (dpy, root_window, 0, 0,
					 img->icn_w, img->icn_h, 
					 0, img->dpy_depth,
					 InputOutput, img->dpy_visual,
					 xswa_mask, &xswa);

	size_hints.flags = PMinSize | PMaxSize;
	size_hints.min_width = img->icn_w;
	size_hints.max_width = img->icn_w;
	size_hints.min_height = img->icn_h;
	size_hints.max_height = img->icn_h;

	XSetNormalHints(dpy, img->icn_window, &size_hints);
	XStoreName(dpy, img->icn_window, img->title);
    }

    if ( !img->gc )
    {
	gc_mask = 0;
	gc_values.function = GXcopy;		gc_mask |= GCFunction;
	gc_values.plane_mask = AllPlanes;	gc_mask |= GCPlaneMask;

	gc_values.foreground = img->white_pixel; gc_mask |= GCForeground;
	gc_values.background = img->black_pixel; gc_mask |= GCBackground;

	gc_values.graphics_exposures = False;	gc_mask |= GCGraphicsExposures;

	img->gc = XCreateGC (dpy, img->window, gc_mask, &gc_values);
    }

    if ( !img->icn_pixmap )
    {
	img->icn_pixmap = XCreatePixmap(dpy, img->icn_window, img->icn_w,
					img->icn_h, img->dpy_depth );

	img->icn_gc = XCreateGC(dpy, img->icn_window, gc_mask, &gc_values);
	icn_alloc = True;
    }

    if ( new_pixmaps || icn_alloc )
	check_pixmap_allocation( img );

    if ( new_window )
    {
	if ( img->icn_pixmap && img->icn_window ) {
	    wm_hints.flags = ( StateHint | IconPixmapHint | IconMaskHint |
			       IconWindowHint | InputHint );
	    wm_hints.initial_state = NormalState;
	    wm_hints.icon_pixmap = img->icn_pixmap;
	    wm_hints.icon_window = img->icn_window;
	    wm_hints.icon_mask = img->icn_pixmap;
	    wm_hints.input = True;

	    XSetWMHints (dpy, img->window, &wm_hints);
	}

	XSetWindowBackgroundPixmap(dpy, img->icn_window, img->icn_pixmap);

	XMapWindow( dpy, img->window );
	XClearWindow( dpy, img->window );

	get_cursors( img->window );
    }

    XFlush( dpy );
}

/*
 * Get the most contrasting colors from the colormap.
 */
static void
get_most_contrasting( img, lightest, darkest )
register image_information *img;
unsigned long *lightest, *darkest;
{
    register int c, l, d;
    long li, di, i;
    int n = img->dpy_visual->map_entries;
    XColor *cells = (XColor *)malloc( n * sizeof(XColor) );

    for ( c = 0; c < n; c++ )
	cells[c].pixel = c;
    XQueryColors( dpy, img->colormap, cells, n );

    /* Find lightest & darkest. */
    d = l = 0;
    di = li = cells[0].red + cells[0].green + cells[0].blue;
    for ( c = 0; c < n; c++ )
    {
	i = cells[c].red + cells[c].green + cells[c].blue;
	if ( i < di )
	{
	    d = c;
	    di = i;
	}
	if ( i > li )
	{
	    l = c;
	    li = i;
	}
    }
    *lightest = cells[l].pixel;
    *darkest = cells[d].pixel;

    free( cells );
}

static void set_white_black_pixel( img )
image_information *img;
{
    static cmap_info cmap_i;
    cmap_i = img->x_cmap;
    if ( img->binary_img || img->mono_color ) {
	if ( img->colormap )
	    get_most_contrasting( img, &img->white_pixel, &img->black_pixel );
	else
	{
	    img->black_pixel = BlackPixel (dpy, screen);
	    img->white_pixel = WhitePixel (dpy, screen);
	}

    }
    else if ( img->pixel_table != NULL ) {
	img->black_pixel = img->pixel_table[0];
	img->white_pixel =
	    img->pixel_table[(img->mono_img) ? img->lvls - 1 :
			     img->lvls * img->lvls * img->lvls - 1];
    }
    else {
	img->black_pixel = 0;
	img->white_pixel =
	    SHIFT_MASK_PIXEL(img->lvls - 1,img->lvls - 1,img->lvls - 1);
    }
}


/* this is a relic */
static int		log2_levels;

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

#define	IS_BINARY	1
#define	IS_MONO		2
#define	WANT_READ_WRITE	4
#define NOT_BINARY 	0
#define NOT_MONO 	0
#define	WANT_READ_ONLY	0

void init_color( img )
register image_information *img;
{
    register int	type;
    Boolean	done = False, try_ro = True;

    while ( ! done ) {
	DPRINTF (stderr, "Cmap type: binary %d, mono %d, read/write %d\n",
		 img->binary_img, img->mono_img, img->rw_cmap );

	type = ((( img->binary_img ) ? IS_BINARY : NOT_BINARY ) |
		(( img->mono_img ) ? IS_MONO : NOT_MONO ) | 
		((( img->rw_cmap && img->sep_colors ) ||
		  ( img->rw_cmap && !try_ro)) ?
		 WANT_READ_WRITE : WANT_READ_ONLY ));

	switch ( type ) {

	case NOT_BINARY | NOT_MONO | WANT_READ_WRITE:
	    done = (( img->sep_colors ) ?
		    init_separate_color_rw( img ) :
		    init_color_rw( img ));
	    if ( !done && !try_ro) {
		fprintf( stderr,
			"%s: not enough cmap entries available, trying b/w.\n",
			 progname );
		img->mono_img = True;
		img->dpy_channels = 1;
	    }
	    break;

	case NOT_BINARY | NOT_MONO | WANT_READ_ONLY:
	    done = (( img->sep_colors ) ?
		    init_separate_color_ro( img, try_ro ) :
		    init_color_ro( img, try_ro ));
	    if ( !done && !try_ro ) {
		fprintf( stderr,
			"%s: not enough cmap entries available, trying b/w.\n",
			 progname );
		img->mono_img = True;
		img->dpy_channels = 1;
	    }
	    break;

	case NOT_BINARY | IS_MONO | WANT_READ_WRITE:
	    done = (img->sep_colors ?
		    init_separate_color_rw( img ) : init_mono_rw( img ));
	    if ( !done & !try_ro ) img->binary_img = True;
	    break;

	case NOT_BINARY | IS_MONO | WANT_READ_ONLY:
	    done = ( img->sep_colors ?
		    init_separate_color_ro( img, try_ro ) :
		    init_mono_ro( img, try_ro ));
	    if ( !done & !try_ro ) img->binary_img = True;
	    break;

	case IS_BINARY | IS_MONO | WANT_READ_ONLY:
	    /* make the magic square */
	    get_dither_arrays( img );
	    make_square( 255.0, img->divN, img->modN, img->dm16 );

	    img->lvls = 2;
	    img->lvls_squared = 4;
	    done = True;
	    break;

	default:
	    fprintf (stderr, "Unknown type in init_colors: %d\n", type);
	    exit (1);
	}
	if (! done) {
	    try_ro = False;
	    if ( img->rw_cmap == True ) {
		get_x_colormap( img );
	    }
	}
    }

    /* If not dithering, center the quantization intervals. */
    if ( !img->dither_img )
    {
	int n = (int)(0.5 + 255.0 / (2 * (img->lvls - 1)));
	register int i, j;
	register int *divN = img->divN;

	for ( i = 0, j = n; j <= 255; i++, j++ )
	    divN[i] = divN[j];
	for ( ; i < 255; i++ )
	    divN[i] = divN[255];
    }

    VPRINTF (stderr, "Created color map with %d entries", img->lvls);

    if (! img->mono_img ) {
	VPRINTF (stderr, " per color, %d total\n", img->lvls * img->lvls * img->lvls);}
    else VPRINTF (stderr, "\n");

    set_white_black_pixel( img );
}

/* Used to allocate the colormap for a DirectColor visual. */

static
Boolean
init_separate_color_rw ( img )
image_information *img;
{
    register int i;
    int log2_num_lvls, num_lvls, total_levels;
    int *map;
    XColor *color_defs;
    cmap_info cmap_i;

    /*
     * use XAllocColorPlanes to allocate all the cells we need --
     * this makes it simple for me.
     */

    img->pixel_table = NULL;
    map = NULL;
    color_defs = NULL;

    for (log2_num_lvls = log2_levels;
	 log2_num_lvls >= 1;
	 log2_num_lvls-- ) {

	num_lvls = 1 << log2_num_lvls;
	total_levels =  1 << (log2_num_lvls * 3);

	if (map == NULL) {
	    map = (int *) malloc (num_lvls * sizeof (int) );
	    if (map == NULL) continue;
	}

	if (color_defs == NULL) {
	    color_defs = (XColor *) malloc ( num_lvls * sizeof (XColor) );
	    if (color_defs == NULL)
		continue;
	}

	if ( XAllocColorPlanes (dpy, img->colormap, 1, &cmap_i.pixel_base, 1,
				log2_num_lvls, log2_num_lvls,
				log2_num_lvls,
				&cmap_i.red_mask,
				&cmap_i.green_mask,
				&cmap_i.blue_mask) == 0)
	    continue;

	if (log2_num_lvls == 8) img->dither_img = False;

	get_dither_arrays( img );
	bwdithermap ( num_lvls, display_gamma, map,
		      img->divN, img->modN, img->dm16 );

	cmap_i.red_shift   = shift_match_right(cmap_i.red_mask);
	cmap_i.green_shift = shift_match_right(cmap_i.green_mask);
	cmap_i.blue_shift  = shift_match_right(cmap_i.blue_mask);
	img->x_cmap = cmap_i;

	/*
	 * Set up the color map entries.
	 */

	for (i = 0; i < num_lvls; i++)
	{
	    color_defs[i].pixel = SHIFT_MASK_PIXEL(i, i, i);

	    color_defs[i].red   = map[i] << 8;
	    color_defs[i].green = map[i] << 8;
	    color_defs[i].blue  = map[i] << 8;

	    color_defs[i].flags = DoRed | DoGreen | DoBlue;
	}

	XStoreColors (dpy, img->colormap, color_defs, num_lvls);

	if (img->lvls != num_lvls) {
	    img->lvls = num_lvls;
	    img->lvls_squared = num_lvls * num_lvls;
	    log2_levels = log2_num_lvls;
	}

	/* pack into a pixel the pre shifted and masked pixel values */
	if ( img->mono_color ) {
	    int shift = 8 - log2_num_lvls;
	    img->pixel_table = (Pixel *)malloc(img->cmlen * sizeof (Pixel));
	    if (!img->pixel_table) continue;
	    for ( i = 0; i < img->cmlen; i++ )
		img->pixel_table[i] =
		    SHIFT_MASK_PIXEL(img->in_cmap[RLE_RED][i] >> shift,
				     img->in_cmap[RLE_GREEN][i] >> shift,
				     img->in_cmap[RLE_BLUE][i] >> shift);
	}

	free (map);
	free (color_defs);

	return (True);

    }

    return (False);
}

/*
 * Used for allocating the colormap in a PsuedoColor visual or other color
 * type. Used for the standard 8 bit display
 */
static
Boolean
init_color_rw ( img )
image_information *img;
{
    int	num_lvls;
    int *map;
    XColor *color_defs, *color;
    int total_levels;
    int	red_index;
    int	green_index;
    int	blue_index;
    int	i, j;
    int free_pixels = 0, shift, cmap_size;
    Pixel *pixels;

    DPRINTF(stderr, "In init_color_rw\n");

    /* get free pixels from the default colormap */
    cmap_size = (1 << img->dpy_depth);

    /* get all free cells in the colormap in <depth> calls to AllocColor */
    pixels = (Pixel *) malloc (cmap_size * sizeof (Pixel) );
    color_defs = (XColor *) malloc (cmap_size * sizeof (XColor) );
    if ( pixels && color_defs )
    {
	DPRINTF(stderr, "Allocating colors: success/failure ");
	for (shift = img->dpy_depth; shift >= 0; shift--)
	    if (XAllocColorCells (dpy, img->colormap, False, 0, 0,
				  &pixels[free_pixels],
				  (int)1 << shift))
	    {
		DPRINTF(stderr, "1");
		free_pixels += (unsigned int)1 << shift;
	    }
	    else DPRINTF(stderr, "0");
	DPRINTF(stderr, " got %d\n", free_pixels);
    }

    total_levels = 1;

    for (num_lvls = img->lvls; 
	 num_lvls > 1; 
	 num_lvls-- )
    {
	total_levels =  num_lvls * num_lvls * num_lvls;
	if ( total_levels <= free_pixels )
	    break;
    }
    if ( !num_lvls )
	return (False);

    if (img->lvls > num_lvls) {
	img->lvls = num_lvls;
	img->lvls_squared = num_lvls * num_lvls;
	for (log2_levels = 1, i = 2; i < img->lvls; i <<= 1, log2_levels++) {
	    /* do nothing */
	}
    }

    if ( num_lvls == 256 )
	img->dither_img = False;

    if (! img->pixel_table )
	img->pixel_table = (Pixel *) malloc (total_levels * sizeof (Pixel) );

    map = (int *) malloc (num_lvls * sizeof (int) );

    get_dither_arrays( img );
    bwdithermap ( num_lvls, display_gamma, map,
		  img->divN, img->modN, img->dm16 );

    /*
     * Use the top free_pixels NOT the bottom ones.  This makes it right for
     * 99% of the DISPLAYs out there which begin using colors at the bottom.
     * We take the top color cells, and copy the bottom ones into the extra
     * spaces, *some* other windows won't be clobbered by our colormap.
     */
    shift = free_pixels - total_levels;

    for (j = 0, i = shift; j < total_levels; i++, j++)
	img->pixel_table[j] = pixels[i];

    for (i = 0; i < shift; i++)
	color_defs[i].pixel = pixels[i];

    XQueryColors( dpy, DefaultColormap(dpy, screen),
		  color_defs, shift );

    /*
     * Set up the color map entries.
     */

    color = &color_defs[shift];

    red_index = 0;
    green_index = 0;
    blue_index = 0;

    for (i = 0; i < total_levels; i++) {
	color->pixel = img->pixel_table[i];
	color->red   = map[red_index] << 8;
	color->green = map[green_index] << 8;
	color->blue  = map[blue_index] << 8;

	color->flags = DoRed | DoGreen | DoBlue;
	color++;

	if (++red_index >= num_lvls) {
	    if (++green_index >= num_lvls) {
		++blue_index;
		green_index = 0;
	    }
	    red_index = 0;
	}

    }

    XStoreColors (dpy, img->colormap, color_defs, free_pixels);

    free (map);
    free (color_defs);
    free (pixels);
    return (True);
}

static
Boolean
init_separate_color_ro ( img, try_ro )
image_information *img;
int try_ro;
{
    cmap_info cmap_i;
    register int	num_lvls;
    register int	log2_num_lvls;
    register int	*map = NULL;
    register int	i;

    cmap_i = img->x_cmap;
    DPRINTF(stderr, "In init_separate_color_ro\n");

    for (log2_num_lvls = log2_levels;
	 log2_num_lvls >= 1;
	 log2_num_lvls-- ) {

	num_lvls = 1 << log2_num_lvls;

	if (map == NULL) {
	    map = (int *) malloc (num_lvls * sizeof (int) );
	    if (map == NULL) continue;
	}

	if (num_lvls == 256) img->dither_img = False;

	DPRINTF (stderr, "num_lvls = %d\n", num_lvls);

	get_dither_arrays( img );
	bwdithermap( num_lvls, display_gamma, map,
		    img->divN, img->modN, img->dm16 );

	cmap_i.red_mask   = img->dpy_visual->red_mask;
	cmap_i.green_mask = img->dpy_visual->green_mask;
	cmap_i.blue_mask  = img->dpy_visual->blue_mask;

	cmap_i.red_shift   = shift_match_left(cmap_i.red_mask, log2_num_lvls);
	cmap_i.green_shift = shift_match_left(cmap_i.green_mask, log2_num_lvls);
	cmap_i.blue_shift  = shift_match_left(cmap_i.blue_mask, log2_num_lvls);

	img->x_cmap = cmap_i;

	if (img->lvls > num_lvls) {
	    img->lvls = num_lvls;
	    img->lvls_squared = num_lvls * num_lvls;
	    log2_levels = log2_num_lvls;
	}

	/* pack into a pixel the pre shifted and masked pixel values */
	if ( img->mono_color ) {
	    int shift = 8 - log2_num_lvls;
	    img->pixel_table = (Pixel *)malloc(img->cmlen * sizeof (Pixel));
	    if (!img->pixel_table) continue;
	    for ( i = 0; i < img->cmlen; i++ )
		img->pixel_table[i] =
		    SHIFT_MASK_PIXEL(img->in_cmap[RLE_RED][i] >> shift,
				     img->in_cmap[RLE_GREEN][i] >> shift,
				     img->in_cmap[RLE_BLUE][i] >> shift);
	}

	if ( map )
	    free (map);

	return (True);

    }

    return (False);

}

static int pixcompare(pixel1, pixel2 )
Pixel *pixel1, *pixel2;
{
    return( *pixel1 - *pixel2 );
}

void free_unique_colors( img, pixels, npixels )
image_information *img;
Pixel *pixels;
int npixels;
{
    Pixel *p;
    int i, nunique;

    if ( no_color_ref_counts )
    {
	qsort(pixels, npixels, sizeof(Pixel), pixcompare);
	p = pixels;

	for (i=1; i<npixels; i++ ) {
	    if ( pixels[i] != *p ) {
		p += 1;
		*p = pixels[i];
	    }
	}

	nunique = (p - pixels) + 1;

	DPRINTF(stderr, "In free_unique_pixels \n\nPixels: ");
	for (i=0;i<nunique;i++) DPRINTF(stderr, " %zu ",pixels[i]);
	DPRINTF(stderr, "\n");
    }
    else
	nunique = npixels;

    XFreeColors(dpy, img->colormap, pixels, nunique, 0);
}

static
Boolean
init_color_ro ( img, try_ro )
image_information *img;
int try_ro;
{
    register int	num_lvls;
    register int	total_levels;
#ifdef XLIBINT_H_NOT_AVAILABLE
    XColor		color_def;
#else
    register XColor	*color_defs = NULL;
    register Status 	*status = NULL;
#endif
    register int	red_index;
    register int	green_index;
    register int	blue_index;
    register int	*map = NULL;
    register int	i;
#ifndef XLIBINT_H_NOT_AVAILABLE
    register int	j;
#endif
    int first = True;

    DPRINTF(stderr, "In init_color_ro\n");

    for (num_lvls = img->lvls; num_lvls >= 2; num_lvls -- ) {

	if (try_ro && !first && specified_levels) break;
	else first = False;

	total_levels = num_lvls * num_lvls * num_lvls;

	if ( !img->pixel_table ) {
	    img->pixel_table = (Pixel *)malloc(total_levels * sizeof (Pixel) );
	    if ( !img->pixel_table ) continue;
	}

	if ( map == NULL ) {
	    map = (int *) malloc (num_lvls * sizeof (int) );
	    if (map == NULL) continue;
	}

	if (num_lvls == 256) img->dither_img = False;

	get_dither_arrays( img );
	bwdithermap( num_lvls, display_gamma, map,
		    img->divN, img->modN, img->dm16 );

	/* try to get a color map entry for each color. */
	red_index = green_index = blue_index = 0;

#ifndef XLIBINT_H_NOT_AVAILABLE
	for ( i = 0; i < total_levels; i++ ) {
	    color_defs->red   = map[red_index] << 8;
	    color_defs->green = map[green_index] << 8;
	    color_defs->blue  = map[blue_index] << 8;

	    if ( XAllocColor (dpy, img->colormap, color_defs ) == 0 ) {
		break;
	    }

	    if (++red_index >= num_lvls) {
		if (++green_index >= num_lvls) {
		    ++blue_index; green_index = 0;
		}
		red_index = 0;
	    }

	    img->pixel_table[i] = color_defs->pixel;
	}

	/* Check if the colors are available */
	if ( i < total_levels ) { 	/* Free the colors already obtained */
	    free_unique_colors (img, img->pixel_table, i );
	    continue;
	}
#else
	if (!color_defs)
	    color_defs = (XColor *) malloc(total_levels * sizeof (XColor));
	if (!status)
	    status = (Status *) malloc(total_levels * sizeof (Status));
	if ( !status || !color_defs ) continue;

	for ( i = 0; i < total_levels; i++ ) {
	    color_defs[i].red   = map[red_index] << 8;
	    color_defs[i].green = map[green_index] << 8;
	    color_defs[i].blue  = map[blue_index] << 8;
	    if (++red_index >= num_lvls) {
		if (++green_index >= num_lvls) {
		    ++blue_index; green_index = 0;
		}
		red_index = 0;
	    }
	}

	if (XAllocColor(dpy, img->colormap, color_defs, total_levels, status)== 0)
	{
	    for ( i = 0, j = 0; i < total_levels; i++ )
		if (status[i])
		    img->pixel_table[j++] = color_defs[i].pixel;
	    free_unique_colors (img, img->pixel_table, j );
	    continue;
	} else
	    for ( i = 0; i < total_levels; i++ )
		img->pixel_table[i] = color_defs[i].pixel;

#endif
	img->lvls = num_lvls;
	img->lvls_squared = num_lvls * num_lvls;

	for (log2_levels = 1, i = 2; i < img->lvls; i <<= 1, log2_levels++) {
	    /* do nothing */
	}
	if (map) free (map);
#ifndef XLIBINT_H_NOT_AVAILABLE
	if (color_defs) free (color_defs);
	if (status) free (status);
#endif
	return (True);
    }
    if (map) free (map);
#ifndef XLIBINT_H_NOT_AVAILABLE
    if (color_defs) free (color_defs);
    if (status) free (status);
#endif
    return (False);
}

static
Boolean
init_mono_rw ( img )
image_information *img;
{
    int	num_lvls;
    int	*map;
    XColor *color_defs, *color;
    int	i, j;
    int free_pixels = 0, shift, cmap_size;
    Pixel *pixels;

    DPRINTF(stderr, "In init_mono_rw\n");

    /* get free pixels from the default colormap */
    cmap_size = (1 << img->dpy_depth);

    /* get all free cells in the colormap in <depth> calls to AllocColor */
    pixels = (Pixel *) malloc (cmap_size * sizeof (Pixel) );
    color_defs = (XColor *) malloc (cmap_size * sizeof (XColor) );
    if ( pixels && color_defs ) {
	DPRINTF(stderr, "Allocating colors: success/failure ");
	for (shift = img->dpy_depth; shift >= 0; shift--)
	    if (XAllocColorCells (dpy, img->colormap, False, 0, 0,
				  &pixels[free_pixels],
				  (int)1 << shift))
	    {
		DPRINTF(stderr, "1");
		free_pixels += (unsigned int)1 << shift;
	    }
	    else DPRINTF(stderr, "0");
	DPRINTF(stderr, " got %d\n", free_pixels);
    }

    if ( img->mono_color ) {
	img->lvls = img->cmlen;
	if ( img->cmlen > free_pixels )
	    fprintf(stderr, "%s: Warning: Not enough cells for input cmap\n",
		    progname);
    }

    if ( free_pixels == 0 ) return False;

    num_lvls = img->lvls;
    while ( num_lvls > free_pixels && num_lvls >= 2 )
	num_lvls = ((num_lvls > 16) ? num_lvls / 2 : num_lvls - 1);

    img->lvls = num_lvls;
    img->lvls_squared = num_lvls * num_lvls;
    for (log2_levels = 1, i = 2; i < img->lvls; i <<= 1, log2_levels++) {
	/* do nothing */
    }

    if ( num_lvls == 256 && img->mono_color )
	img->dither_img = False;

    if ( !img->pixel_table )
	img->pixel_table = (Pixel *)
	    malloc ((img->mono_color? img->cmlen : num_lvls) * sizeof(Pixel) );
    map = (int *) malloc ( num_lvls * sizeof(int) );

    if (!map || !img->pixel_table)  {
	fprintf(stderr, "malloc problems in init_mono_rw\n");
	return False;
    }

    get_dither_arrays( img );
    bwdithermap(num_lvls, display_gamma, map,
		img->divN, img->modN, img->dm16 );

    /*
     * Use the top free_pixels NOT the bottom ones.  This makes it right for
     * 99% of the DISPLAYs out there which begin using colors at the bottom.
     * We take the top color cells, and copy the bottom ones into the extra
     * spaces, *some* other windows won't be clobbered by our colormap.
     */
    shift = free_pixels - num_lvls;

    for (j = 0, i = shift; j < num_lvls; i++, j++)
	img->pixel_table[j] = pixels[i];

    for (i = 0; i < shift; i++)
	color_defs[i].pixel = pixels[i];

    XQueryColors( dpy, DefaultColormap(dpy, screen), color_defs, shift );

    /*
     * Set up the color map entries.
     */

    color = &color_defs[shift];

    if (img->mono_color)
	for ( i = 0; i < num_lvls; i++ ) {
	    color->pixel = img->pixel_table[i];
	    color->red = img->in_cmap[RLE_RED][i] << 8;
	    color->green = img->in_cmap[RLE_GREEN][i] << 8;
	    color->blue = img->in_cmap[RLE_BLUE][i] << 8;
	    color->flags = DoRed | DoGreen | DoBlue;
	    color++;
	}
    else
	for ( i = 0; i < num_lvls; i++ ) {
	    color->pixel = img->pixel_table[i];
	    color->red = map[i] << 8;
	    color->green = map[i] << 8;
	    color->blue = map[i] << 8;
	    color->flags = DoRed | DoGreen | DoBlue;
	    color++;
	}

    XStoreColors (dpy, img->colormap, color_defs, free_pixels);

    free (map);
    free (color_defs);
    free (pixels);
    return (True);
}

static
Boolean
init_mono_ro ( img, try_ro )
image_information *img;
int try_ro;
{
    register int		num_lvls;
#ifdef XLIBINT_H_NOT_AVAILABLE
    XColor			color_def;
#else
    register XColor		*color_defs = NULL;
    register Status 		*status = NULL;
#endif
    register int		* map = NULL;
    register int		i;
#ifndef XLIBINT_H_NOT_AVAILABLE
    register int		j;
#endif
    int first = True;

    DPRINTF(stderr, "In init_mono_ro");
    if ( try_ro ) {
	DPRINTF(stderr, " trying read/only\n");
    } else {
	DPRINTF(stderr, "\n");
    }

    if ( img->mono_color )
	img->lvls = img->cmlen;

    for (num_lvls = img->lvls; num_lvls >= 2;
	 num_lvls = (num_lvls > 16) ? num_lvls / 2 : num_lvls - 1 ) {

	if ( try_ro && !first && specified_levels ) break;
	else if ( !first && img->mono_color ) break;
	else first = False;

	if (img->pixel_table == NULL) {
	    img->pixel_table = (Pixel *) malloc (num_lvls * sizeof (Pixel) );
	    if (img->pixel_table == NULL) continue;
	}

	if (map == NULL) {
	    map = (int *) malloc (num_lvls * sizeof (int) );
	    if (map == NULL) continue;
	}

	if (num_lvls == 256 || img->mono_color)
	    img->dither_img = False;

	get_dither_arrays( img );
	bwdithermap(num_lvls, display_gamma, map,
		    img->divN, img->modN, img->dm16 );

	/* try to get a color map entry for each color.  */

#ifndef XLIBINT_H_NOT_AVAILABLE
	for ( i = 0; i < num_lvls; i++ ) {
	    if ( img->mono_color ) {
		color_defs->red   = img->in_cmap[RLE_RED][i] << 8;
		color_defs->green = img->in_cmap[RLE_GREEN][i] << 8;
		color_defs->blue  = img->in_cmap[RLE_BLUE][i] << 8;
	    } else {
		color_defs->red   = map[i] << 8;
		color_defs->green = map[i] << 8;
		color_defs->blue  = map[i] << 8;
	    }

	    if ( XAllocColor (dpy, img->colormap, color_defs ) == 0 ){
		break;
	    }
	    img->pixel_table[i] = color_defs->pixel;
	}

	/* Check if the colors are available */

	if ( i < num_lvls) {
	    /* Free the colors already obtained */

	    free_unique_colors (img, img->pixel_table, i );
	    continue;		/* adjust level & repeat */
	}
#else
	if (!color_defs)
	    color_defs = (XColor *) malloc(num_lvls * sizeof (XColor));
	if (!status)
	    status = (Status *) malloc(num_lvls * sizeof (Status));
	if ( !status || !color_defs ) continue;

	for ( i = 0; i < num_lvls; i++ ) {
	    color_defs[i].pixel = 0;
	    if ( img->mono_color ) {
		color_defs[i].red   = img->in_cmap[RLE_RED][i] << 8;
		color_defs[i].green = img->in_cmap[RLE_GREEN][i] << 8;
		color_defs[i].blue  = img->in_cmap[RLE_BLUE][i] << 8;
	    } else {
		color_defs[i].red   = map[i] << 8;
		color_defs[i].green = map[i] << 8;
		color_defs[i].blue  = map[i] << 8;
	    }
	}

	if (XAllocColor(dpy, img->colormap, color_defs, num_lvls, status)== 0)
	{
	    for ( i = 0, j = 0; i < num_lvls; i++ )
		if (status[i])
		    img->pixel_table[j++] = color_defs[i].pixel;
	    free_unique_colors (img, img->pixel_table, j );
	    continue;
	} else
	    for ( i = 0; i < num_lvls; i++ )
		img->pixel_table[i] = color_defs[i].pixel;

#endif
	img->lvls = num_lvls;
	img->lvls_squared = num_lvls * num_lvls;

	for (log2_levels = 1, i = 2; i < img->lvls; i <<= 1, log2_levels++);
	/* do nothing */

	free (map);
#ifndef XLIBINT_H_NOT_AVAILABLE
	if (status) free (status);
	if (color_defs) free (color_defs);
#endif
	return (True);

    }
    if (img->pixel_table)
	free (img->pixel_table);
    img->pixel_table = NULL;

    if (map) free (map);
#ifndef XLIBINT_H_NOT_AVAILABLE
    if (status) free (status);
    if (color_defs) free (color_defs);
#endif
    return (False);
}

void free_image_colors( img )
image_information *img;
{
    int ncells = img->mono_img ? img->lvls : img->lvls*img->lvls*img->lvls;
    if ( img->rw_cmap && !img->binary_img )
	XFreeColors(dpy, img->colormap, img->pixel_table, ncells, 0);
}

static CONST_DECL char *visual_class_to_string(visual_type)
    int visual_type;
{
    CONST_DECL char *type_string;
    switch (visual_type) {
    case DirectColor:	type_string = "DirectColor";	break;
    case PseudoColor:	type_string = "PseudoColor";	break;
    case TrueColor:	type_string = "TrueColor";	break;
    case StaticColor:	type_string = "StaticColor";	break;
    case GrayScale:	type_string = "GrayScale";	break;
    case StaticGray:	type_string = "StaticGray";	break;
    default: 		type_string = "any/unknown";	break;
    }
    return type_string;
}

#define BINARY_TABLE_INDEX	0
#define MONOCHROME_TABLE_INDEX	1
#define COLOR_TABLE_INDEX	2

/* Here we ALWAYS want to take a PseudoColor over the next best visual... */
/* in Monochrome, we can do just as much with a pseudo color visual, and   */
/* we may be able to preserve some of the default colors from other windows*/
/* In color mode, some eight bit displays offer a Direct and TrueColor     */
/* visual..   This is quite confusing, Have you ever seen a 24bit display  */
/* offer a 24-bit PseudoColor visual?  They offer 8 bit PC visuals usually */

static int desired_class_table [][6] = {
{ PseudoColor, StaticGray, GrayScale, DirectColor, TrueColor, StaticColor},
{ PseudoColor, GrayScale, StaticGray, DirectColor, TrueColor, StaticColor},
{ PseudoColor, DirectColor, TrueColor, StaticColor, GrayScale, StaticGray}
};

void
find_appropriate_visual( img )
register image_information *img;
{
    static int			num_visuals = 0;
    static XVisualInfo		*visual_info = NULL;
    register XVisualInfo	*vi, *found_vi;
    XVisualInfo			*XGetVisualInfo ();
    VisualID			def_visual_id;
    int				def_visual_index = 0;
    int				def_scrn = DefaultScreen(dpy);
    int				desired_class;
    int				desired_depth;
    int 			deepest_visual;
    int				depth_delta;
    register int		i;

    DPRINTF(stderr, "In find_appropriate_visual(%d)\n", img->img_channels);

    if (visual_info == NULL) {
	visual_info = XGetVisualInfo (dpy, VisualNoMask, NULL, &num_visuals);
	if (visual_info == NULL || num_visuals == 0) {
	    fprintf (stderr, "XGetVisualInfo failed\n");
	    exit (1);
	}
    }

    desired_depth = 1;

    for (i = 2; i < img->lvls; i <<= 1) {
	desired_depth++;
    }

    if ( img->mono_img && img->lvls == 2 )
	img->binary_img = True;

    if ( img->binary_img ) {
	desired_class = BINARY_TABLE_INDEX;
	desired_depth = 1;
	depth_delta = 1;
    }
    else if ( img->mono_img && !img->mono_color ) {
	desired_class = MONOCHROME_TABLE_INDEX;
	depth_delta = 1;
    }
    else {
	desired_class = COLOR_TABLE_INDEX;
	desired_depth *= 3;		/* needed if separate colors	*/
	depth_delta = 3;
    }

    VPRINTF (stderr, "Searching for %s visual with desired depth >= %d\n", 
	     visual_class_to_string(img->visual_class), desired_depth);

    /*
     * find visual such that:
     *
     *	1. depth is as large as possible (up to true desired depth)
     *	2. visual depth is the smallest of those supported >= depth
     *	3. visual class is the `most desired' for the image type
     *		Meaning! that if it is the DefaultVisual it IS the
     *		most desired.  We can't choose one visual class over
     *		another for all displays!  If the depth of the DefaultVisual
     *		is large enough for the image, or as large as all others, we
     *		use it!  This minimizes screw ups on our part.
     *  4. If specified_screen is >=0, visual must be on that screen.
     *  5. Prefer visuals on default screen.
     */

    found_vi = NULL;

    def_visual_id = DefaultVisual(dpy, def_scrn)->visualid ;
    deepest_visual = 0;

    for (i = 0; i < num_visuals; i++)
    {
	if ( deepest_visual < visual_info[i].depth )
	    deepest_visual = visual_info[i].depth;

	if ( def_visual_id == visual_info[i].visualid &&
	     def_scrn == visual_info[i].screen )
	    def_visual_index = i;
    }

    /* Take the Default Visual if it's cool enough... */
    if ( visual_info[def_visual_index].depth >= desired_depth ||
	 visual_info[def_visual_index].depth == deepest_visual )
	found_vi = &visual_info[def_visual_index];

    /* if we were told to look for a specific type first, do it */
    if ( img->visual_class >= 0 ) {
	int depth;
	for (depth = desired_depth; depth >= 1; depth -= depth_delta) {
	    for (vi = visual_info; vi < visual_info + num_visuals; vi++) 
		if (vi->class == img->visual_class && vi->depth >= depth &&
		    (specified_screen < 0 || vi->screen == specified_screen) )
		{
		    found_vi = vi;

		    if (found_vi->depth == depth) break;
		}
	    if (found_vi != NULL && found_vi->class == img->visual_class)
		break;
	}
    }

    /* Werent told to get any type of Visual, or didn't find one, wingit */
    if ( img->visual_class < 0 ) {
	int depth;
	for (depth = desired_depth; depth >= 0; depth -= depth_delta) {
	    if (found_vi != NULL)
		break;
	    for (i = 0; i < 6; i++) {
		int vt;

		/* search for class and depth */
		vt = desired_class_table[desired_class][i];

		for (vi = visual_info; vi < visual_info+num_visuals; vi++)
		    if (vi->class == vt && vi->depth >= depth) {
			if (found_vi==NULL || found_vi->depth > vi->depth)
			    found_vi = vi;
			if (vi->depth == depth)
			    break;
		    }
	    }
	}
    }

    if (found_vi == NULL) {
	fprintf (stderr, "%s: Could not find appropriate visual type - %s\n",
		 progname, visual_class_to_string(img->visual_class));
	exit (1);
    }

    /* Now, if screen not explicitly specified, try to find this visual on
     * the default screen.
     */
    if ( specified_screen < 0 )
	for (vi = visual_info; vi < visual_info+num_visuals; vi++)
	    if (vi->class == found_vi->class &&
		vi->depth == found_vi->depth &&
		vi->screen == def_scrn )
	    {
		found_vi = vi;
		break;
	    }

    /* set program the_hdr */
    screen = found_vi->screen;
    root_window = RootWindow (dpy, screen);

    /* set img variables */
    img->dpy_depth = found_vi->depth;
    img->dpy_visual = found_vi->visual;
    img->visual_class = found_vi->class;

    /* We want to give the DefaultColormap a shot first */
    if ( found_vi->visualid == def_visual_id )
	img->colormap = DefaultColormap( dpy, screen );
    else
	get_x_colormap( img );

    if ( img->dpy_depth == 1 || img->binary_img ) {
	img->binary_img = True;
	img->mono_img = True;
	img->color_dpy = False;
	img->dpy_channels = 1;
	img->sep_colors = False;
	img->rw_cmap = False;

	img->lvls = 2;
	img->lvls_squared = 4;
	log2_levels = 1;
    }
    else {
	int class = img->visual_class;

	if ( class == GrayScale || class == StaticGray ) {
	    img->mono_img = True;
	    img->color_dpy = False;
	    img->dpy_channels = 1;
	    depth_delta = 1;
	}

	img->rw_cmap = class == PseudoColor || class == GrayScale ||
	    class == DirectColor;

	if ( class == DirectColor || class == TrueColor ||
	    (class == StaticColor &&
	     (!img->dither_img || img->dpy_depth > 8) &&
	     (found_vi->visual->red_mask != 0 ||
	      found_vi->visual->blue_mask != 0 ||
	      found_vi->visual->green_mask != 0))) {
	    /* StaticColor could be like a PseudoColor Display with a dither
	     * cube, or like a Direct or TrueColor display with shifts and
	     * masks...  The dither cube would probubly look better on an 8
	     * bit SC display.  For 8 bit or less SC visuals PseudoColor
	     * (dither cube, dithering or no) will be used unless you specify
	     * -a(nodither) on the command line and there are masks provided
	     * in the visual structure.
	     */

	    img->sep_colors = True;

	    i = 1 << (img->dpy_depth / depth_delta);

	    if ( img->lvls > i) {
		img->lvls = i;
		img->lvls_squared = i * i;
	    }
	} else {
	    img->sep_colors = False;

	    /* Image is monochrome??? */
	    if (depth_delta == 1) {
		i = found_vi->colormap_size;
	    }
	    else { /* color */
		for (i = 1; i * i * i < found_vi->colormap_size; i++) {}
		i--;
	    }

	    if (img->lvls > i) {
		img->lvls = i;
		img->lvls_squared = i * i;
	    }
	}

	for (log2_levels = 1, i = 2; i < img->lvls; i <<= 1, log2_levels++);
	/* do nothing */
    }

    VPRINTF(stderr, "Visual type %s, depth %d, screen %d\n",
	    visual_class_to_string(img->visual_class), img->dpy_depth, screen);

    VPRINTF(stderr, "levels: %d, log(2) levels: %d\n", img->lvls, log2_levels);
}

static XFontStruct *pixel_font_info = NULL;

void
MapPixWindow ( img, use_top )
image_information *img;
int use_top;
{
    Window hukairz;
    int x, y;
    unsigned int w, h, hunoz, huwanz, font_height;

    XGetGeometry (dpy, img->window, &hukairz, &x, &y, &w, &h, &hunoz, &huwanz);

    if (!pixel_font_info) {
	pixel_font_info = XLoadQueryFont (dpy, "fixed");
	if (!pixel_font_info) {
	    fprintf (stderr, "%s: unable to load font '%s'\n",
		     progname, "fixed");
	    exit (1);
	}
    }

    font_height = pixel_font_info->ascent + pixel_font_info->descent + 4;
    y = (use_top ? 0 : (h - font_height));

    if ( img->pix_info_window == 0 ) {
	XSetFont (dpy, img->gc, pixel_font_info->fid );
	img->pix_info_window = XCreateSimpleWindow(
	    dpy, img->window, 0, y, w, font_height, 0, None,
	    img->black_pixel );
    }
    else
	XMoveResizeWindow ( dpy, img->pix_info_window, 0, y, w, font_height );

    XMapWindow ( dpy, img->pix_info_window );
}

void
DrawPixWindow ( img, x, y )
image_information *img;
int x, y;
{
    char str[256];
    unsigned char *r, *g, *b;

    r = SAVED_RLE_ROW( img, y ) + x;

    switch ( img->dpy_channels ) {
    case 1:
	if ( img->mono_color && img->in_cmap ) {
	    unsigned char R = img->in_cmap[0][*r];
	    unsigned char G = img->in_cmap[1][*r];
	    unsigned char B = img->in_cmap[2][*r];
	    if ( img->mag_fact > 1 )
		sprintf (str, "(%4d,%4d): %3d -> 0x%02x%02x%02x ( %3d, %3d, %3d ) (Mag %2d)",
			 x + img->x, img->h - y - 1 + img->y,
			 *r, R, G, B, R, G, B, img->mag_fact);
	    else
		sprintf (str, "(%4d,%4d): %3d -> 0x%02x%02x%02x ( %3d, %3d, %3d )",
			 x + img->x, img->h - y - 1 + img->y,
			 *r, R, G, B, R, G, B);
	} else {
	    if ( img->mag_fact > 1 )
		sprintf (str, "(%4d,%4d): ( %3d ) (Mag %2d)",
			 x + img->x, img->h - y - 1 + img->y,
			 *r, img->mag_fact);
	    else
		sprintf (str, "(%4d,%4d): ( %3d )",
			 x + img->x, img->h - y - 1 + img->y, *r);
	}
	break;
    case 2:
	g = r + img->w;
	if ( img->mag_fact > 1 )
	    sprintf (str, "(%4d,%4d): ( %3d, %3d ) (Mag %2d)",
		     x + img->x, img->h - y - 1 + img->y,
		     *r, *g, img->mag_fact);
	else
	    sprintf (str, "(%4d,%4d): ( %3d, %3d )",
		     x + img->x, img->h - y - 1 + img->y,
		     *r, *g);
	break;
    case 3:
	g = r + img->w;
	b = g + img->w;
	if ( img->mag_fact > 1 )
	    sprintf (str,
		     "(%4d,%4d): 0x%02x%02x%02x ( %3d, %3d, %3d ) (Mag %2d)",
		     x + img->x, img->h - y - 1 + img->y,
		     *r, *g, *b, *r, *g, *b, img->mag_fact);
	else
	    sprintf (str, "(%4d,%4d): 0x%02x%02x%02x ( %3d, %3d, %3d )",
		     x + img->x, img->h - y - 1 + img->y,
		     *r, *g, *b, *r, *g, *b);
	break;
    }

    XClearWindow ( dpy, img->pix_info_window );
    XDrawString ( dpy, img->pix_info_window, img->gc,
		  4, 2 + pixel_font_info->ascent, str, strlen (str) );
}

void
DrawSpeedWindow ( img, s )
image_information *img;
int s;
{
    char str[256];

    if ( s > 0 )
	sprintf( str, "%s%d Frames/Second", (s > 0) ? "": "1/", (s>0)?s:-s );
    else
	sprintf( str, "As fast as possible" );

    XClearWindow ( dpy, img->pix_info_window );
    XDrawString ( dpy, img->pix_info_window, img->gc,
		  4, 2 + pixel_font_info->ascent, str, strlen (str) );
}


void
UnmapPixWindow( img )
image_information *img;
{
    XUnmapWindow ( dpy, img->pix_info_window );
}

