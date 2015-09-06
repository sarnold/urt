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
 * getx11.h - Declaration for image_information structure...  (tote a global)
 * 
 * Author:	Martin R. Friedmann 
 * 		Dept of Electrical Engineering and Computer Science
 *		University of Michigan
 * Date:	Tue, Dec 10, 1989
 * Copyright (c) 1989, University of Michigan
 */

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include "rle.h"
#ifdef X_SHARED_MEMORY
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#ifndef USE_PROTOTYPES
extern void_star shmat();
#endif
#endif

#define COUNT_OF(_array_)	( sizeof (_array_) / sizeof (_array_[0]) )
#define IMAGE_BORDERWIDTH	3

typedef int Boolean;
typedef unsigned long Pixel;

#define MALLOC_FAILURE 3
#define FILE_FAILURE 2
#define FATAL_FAILURE 1
#define SUCCESS 0

#define VPRINTF if (verbose_flag) fprintf
#define DPRINTF if (debug_flag) fprintf

#define SHIFT_MASK_PIXEL(r, g, b) \
    ( (( ( (r) << cmap_i.red_shift  ) & cmap_i.red_mask )\
       | ( ( (g) << cmap_i.green_shift) & cmap_i.green_mask )\
       | ( ( (b) << cmap_i.blue_shift ) & cmap_i.blue_mask )) +\
      cmap_i.pixel_base)

#define SHIFT_MASK_PIXEL_32(r, g, b) \
    (( (r) << cmap_i.red_shift  ) \
     | ( (g) << cmap_i.green_shift) \
     | ( (b) << cmap_i.blue_shift ))

#define Min(x,y) (((x) < (y)) ? (x) : (y))
#define Max(x,y) (((x) > (y)) ? (x) : (y))

typedef void VOID_FUNCTION(); 
typedef int array16[16];

extern double		display_gamma;
extern int 		iflag;

extern char 		*progname;
extern Display 		*dpy;
extern Window		root_window;
extern int		screen;

extern Boolean		debug_flag;	/* set if debug mode -D */
extern Boolean		verbose_flag;	/* set if verbose mode -v */
extern int 		stingy_flag;
extern int              specified_levels;
#ifdef X_SHARED_MEMORY
/*
 * use_shared_pixmaps is TRUE if the server supports shared images.
 * no_shared_space is set to TRUE the first time shmget returns
 * ENOSPC, so the error message is only printed once. We still
 * try, though, as an 'r' or 'q' command could release segments.
 */
extern Boolean		do_sharing;
extern Boolean		use_shared_pixmaps;
extern Boolean		no_shared_space;
#endif

/* X11/NeWS server bug workaround. */
extern int no_color_ref_counts;

/* 
 * Color map, gamma correction map, and lookup tables 
 */

typedef struct _cmap_info_struct
{
    int		red_shift;
    int		green_shift;
    int		blue_shift;
    Pixel	red_mask;
    Pixel	green_mask;
    Pixel	blue_mask;
    Pixel	pixel_base;
} cmap_info;

typedef struct _image_info_struct
{
    Window window, icn_window;	/* X windows and pixmaps */
    Window pix_info_window;
    Pixmap pixmap, icn_pixmap;
    GC gc, icn_gc;			/* And GC's and XImages */
    XImage *image, *icn_image;
    Colormap colormap;
    int visual_class;
    Visual *dpy_visual;
    int dpy_depth;
    Boolean pixmap_failed;

    CONST_DECL char *filename;		/* file that Image came from.  */
    CONST_DECL char *title;		/* title for this image...     */
    int   img_num;			/* Number of image within file. */
    FILE *fd;
    unsigned char *scan_data;           /* a[img_h][img_w][img_clr_channels] */
    int img_channels;			/* # of color channels in file       */
    int dpy_channels;			/* # of channels we will display     */
    VOID_FUNCTION *map_scanline;	/* map_scanline routine to use       */
    VOID_FUNCTION *MAG_scanline;	/* MAG_scanline routine to use       */
    float gamma;
    float dpy_gamma;
    
    int x, y;				/* Original origin of image	     */
    int xo, yo;				/* Origin of image in X11 window (-y)*/
    int w, h;				/* width and height of image         */
    int win_w, win_h;			/* width and height of window        */
    int icn_w, icn_h;			/* width and height of icon          */
    int pix_w, pix_h;			/* width and height of img->pixmap   */
    int icn_factor;			/* divide factor from img -> icon    */

    int pan_x, pan_y, pan_w, pan_h;	/* image rect currently being viewed */
    int mag_fact, save_mag_fact;	/* current magnification factor      */
    Boolean mag_mode;			/* are we display magnified image?   */
    int save_pan_x, save_pan_y;
    int save_pan_w, save_pan_h;
    int save_win_w, save_win_h;		/* wdth and hgt of window when saved */
    
    Boolean binary_img;			/* will we make it 2 color? (-W)     */
    Boolean mono_img;			/* do we make it grey scale? (-w)    */
    Boolean dither_img;			/* do we dither it? (-a)             */
    Boolean rw_cmap;			/* is the colormap writable?         */
    Boolean sep_colors;			/* is the visual True or DirectColor?*/
    Boolean mono_color;			/* a one channel color image (mcut)  */
    Boolean color_dpy;			/* False if we are FORCED to b/w     */

    rle_pixel **in_cmap;
    int ncmap;				/* rle_hdr.ncmap 		     */
    int cmlen;				/* Comment `color_map_length` in file*/

    int *modN;				/* dither arrays, all of them */
    int *divN;
    array16 *dm16;			
    cmap_info x_cmap;
    Pixel *pixel_table;
    Pixel white_pixel, black_pixel;
    int lvls, lvls_squared;
#ifdef X_SHARED_MEMORY
    XShmSegmentInfo shm_img;
    XShmSegmentInfo shm_pix;
#endif
} image_information;

/* pointer arithmetic... gack!  */
/* Returns Y'th row in our saved data array.  Works around problem with */
/* rle_getrow by adding 1 to y.  We waste the first line of this array  */
/* SAVED_RLE_ROW(img, -1) == img->scan_data, and is never used...       */
#define SAVED_RLE_ROW( img, y ) \
    ((img)->scan_data + (((y) + 1) * (img)->w * (img)->dpy_channels)) 


#define duff8(counter, block) {\
  while (counter >= 8) {\
     { block; } \
     { block; } \
     { block; } \
     { block; } \
     { block; } \
     { block; } \
     { block; } \
     { block; } \
     counter -= 8;\
  } \
  switch (counter & 7) { \
     case 7:    { block; } \
     case 6:    { block; } \
     case 5:    { block; } \
     case 4:    { block; } \
     case 3:    { block; } \
     case 2:    { block; } \
     case 1:    { block; } \
     case 0:    counter = 0;\
     }\
}

#ifdef USE_PROTOTYPES
/* Prototypes.h contains prototypes for all global functions.  It is
 * automatically generated using the cproto program.
 */
#include "prototypes.h"
#else
/* This is also automatically generated by cproto. */
#include "fn_decls.h"
#endif
