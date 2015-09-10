#ifndef lint
static char     sccsid[] = "@(#)gettaac.c	1.2 7/5/90     Copyright (c) 1989, 1990\
by Southwest Research Institute, San Antonio, Texas";
#endif

/*
Copyright (c) 1989, 1990 Southwest Research Institute
All rights reserved.

Redistribution and use in source and binary forms are permitted
provided that the above copyright notice and this paragraph are
duplicated in all such forms and that any documentation,
advertising materials, and other materials related to such
distribution and use acknowledge that the software was developed
by Southwest Research Institute.  The name of Southwest Research
Institute may not be used to endorse or promote products derived
from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*/

/*
 * Send bug fixes and improvements to:  ksp@maxwell.nde.swri.edu
 */

#include <errno.h>
#include <stdio.h>
#include <strings.h>
#include <signal.h>
#include <suntool/sunview.h>
#include <suntool/panel.h>
#include <suntool/canvas.h>
#include <sunwindow/win_cursor.h>
#include <sunwindow/notify.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <pwd.h>
#include "rle.h"
#include <sys/stat.h>
#include <taac1/taio.h>

#define MAX_FILE_NAME	1024
#define	FONT	"/usr/lib/fonts/fixedwidthfonts/serif.r.14"
#define	NCMAP	256
#define	X_PIXELS	512		/* number of x pixels */
#define	Y_PIXELS	480		/* number of y pixels */

static Frame    base_frame;
static Canvas   canvas;
static Pixwin  *pw;
static Cursor   cursor;


/* control panel */
static Panel    panel;
static void     init_panel();
static Panel_item	file_name;
static Panel_item	gamma_item;

/* panel notify procs */
static void     doexit();
static void     read_file();
static void     format_select();
static void	make_new_name();

/* utilities */
static void     init_color_map();
static void     bw_init_colors();
static void     init_canvas();

/* useful stuff */
static struct pixfont *pixfont;
static unsigned int *scan_line_buffer;
static char *my_name;

static short    icon_image[] = {
#include	"gettaac.icon"
};

DEFINE_ICON_FROM_IMAGE(gettaac_icon, icon_image);

static unsigned char	red[NCMAP];
static unsigned char	green[NCMAP];
static unsigned char	blue[NCMAP];

#define	RGB	0
#define GRAY	1

static int	display_mode = RGB;

static void	read_file_rgb();
static void	read_file_gray();
static void	read_file_bw();

/* taac stuff */
TA_HANDLE	*tahandle;
static int	erase[X_PIXELS];
static Notify_value	taac_interposer();

main(argc, argv)
int             argc;
char          **argv;
{

     register int    i;
     struct pixrect	*screen;

     my_name = cmd_name( argv );

     pixfont = pf_open(FONT);

     if (pixfont == NULL) {
	  fprintf(stderr, "Can't open the font %s\n", FONT);
	  exit(1);
     }

     /* open the taac */
     if((tahandle = ta_open(0))==NULL){
	  fprintf(stderr, "ta_open failed.\n");
	  exit(1);
     }  

     if (ta_init(tahandle) == TA_FAILURE) {
	  fprintf(stderr, "error initializing taac\n");
	  exit(1);
     }
     
     ta_set_video(tahandle,TA_VIDEO_MIXEDTAAC,TA_SYNC_EXTERNAL);



     base_frame = window_create(0, FRAME,
				FRAME_ICON, &gettaac_icon,
				FRAME_ARGC_PTR_ARGV, &argc, argv,
				0);
	
     init_panel(base_frame);


     canvas = window_create(base_frame, CANVAS,
			    WIN_WIDTH, X_PIXELS,
			    WIN_HEIGHT, Y_PIXELS,
			    CANVAS_AUTO_SHRINK, FALSE,
			    0);

     window_fit(base_frame);

     ta_taac_canvas(tahandle, base_frame, canvas);

     window_main_loop(base_frame);
     
     ta_close(tahandle);

}

static void
init_panel(base_frame)
Frame           base_frame;
{

     panel = window_create(base_frame, PANEL,
			   WIN_ROWS, 2,
			   PANEL_LABEL_BOLD, TRUE,
			   0);

     panel_create_item(panel, PANEL_BUTTON,
		       PANEL_ITEM_X, ATTR_COL(1),
		       PANEL_ITEM_Y, ATTR_ROW(0) + 4,
		       PANEL_LABEL_IMAGE, panel_button_image(panel, "EXIT", 4, 0),
		       PANEL_NOTIFY_PROC, doexit,
		       0);

     panel_create_item(panel, PANEL_BUTTON,
		       PANEL_ITEM_X, ATTR_COL(8),
		       PANEL_ITEM_Y, ATTR_ROW(0) + 4,
		       PANEL_LABEL_IMAGE, panel_button_image(panel, "LOAD", 4, 0),
		       PANEL_NOTIFY_PROC, read_file,
		       0);

     panel_create_item(panel, PANEL_CHOICE,
		       PANEL_ITEM_X, ATTR_COL(15),
		       PANEL_ITEM_Y, ATTR_ROW(0) + 4,
		       PANEL_CHOICE_STRINGS, "RGB", "Gray", 0,
		       PANEL_NOTIFY_PROC,	format_select,
		       0);

     gamma_item = panel_create_item(panel, PANEL_TEXT,
				    PANEL_ITEM_X, ATTR_COL(30),
				    PANEL_ITEM_Y, ATTR_ROW(0) + 8,
				    PANEL_VALUE_DISPLAY_LENGTH, 4,
				    PANEL_LABEL_STRING, "Gamma:",
				    PANEL_VALUE, "1.6",
				    0);
	
     file_name = panel_create_item(panel, PANEL_TEXT,
				   PANEL_ITEM_X, ATTR_COL(42),
				   PANEL_ITEM_Y, ATTR_ROW(0) + 8,
				   PANEL_LABEL_STRING, "File:",
				   PANEL_VALUE_STORED_LENGTH, MAX_FILE_NAME,
				   PANEL_NOTIFY_PROC, make_new_name,
				   PANEL_VALUE_STORED_LENGTH, MAX_FILE_NAME,
				   PANEL_NOTIFY_STRING, "\033\t\r",
				   PANEL_VALUE_DISPLAY_LENGTH, 14,
				   0);

     window_set(panel, PANEL_CARET_ITEM, file_name, 0);
}



/* utilites */

static void
doexit()
{
	
     /* close the taac */
     ta_close(tahandle);

     exit(0);
}

static void
read_file()
{
     
     char	*filename;
     char	full_path[MAX_FILE_NAME];
     FILE	*fp;
     rle_hdr	hdr;

     hdr = *rle_hdr_init( (rle_hdr *)NULL );

     filename = (char *)panel_get_value(file_name);

     rle_names( &hdr, my_name, filename );

     if (*filename == '\0') {
	  fp = stdin;
     } else {
	  get_full_path(filename, full_path);
	  if ((fp = fopen(full_path, "r")) == NULL) {
	       fprintf(stderr, "Can't open file ->%s<-\n", full_path);
	       return;
	  }
     }

     hdr.rle_file = fp;
     rle_get_setup_ok( &hdr, NULL, NULL );

     if (display_mode == RGB)
	  read_file_rgb(&hdr);
     else if (display_mode == GRAY)
	  read_file_gray(&hdr);
     else
	  read_file_rgb(&hdr);
	
     fclose (hdr.rle_file);
}

static void
read_file_rgb(hdr)
rle_hdr *hdr;
{

     unsigned char *scan[3];
     register unsigned char	*r, *g, *b;
     register int	x, y;
     unsigned char	*rmap, *gmap, *bmap;
     rle_pixel	**in_cmap;
     double	gamma;
     int	x_size, y_size;
     int i;

     RLE_CLR_BIT(*hdr, RLE_ALPHA);

     x_size = (hdr->xmax - hdr->xmin + 1);
     y_size = (hdr->ymax - hdr->ymin + 1);

     /* get memory for the scan line data */
     for (i = 0; i < 3; i++)
	  scan[i] = (unsigned char *) malloc(x_size);

     hdr->xmax -= hdr->xmin;
     hdr->xmin = 0;

     if (sscanf((char *)panel_get_value(gamma_item), "%lf", &gamma) != 1)
	  gamma = 1.0;

     in_cmap = buildmap( hdr, 3, 1.0 / gamma, 1.0 );

     rmap = &(in_cmap[0][0]);
     gmap = &(in_cmap[1][0]);
     bmap = &(in_cmap[2][0]);

     
     /* get memory for the output buffer */
     scan_line_buffer = (unsigned int *) malloc(x_size * sizeof(unsigned int));


     for ( i = 0; i < Y_PIXELS; i++)
	  ta_write2d(tahandle, erase, X_PIXELS, 1, 0, i);

     while ( (y = rle_getrow( hdr, scan )) <= hdr->ymax ) {
	  for(x = 0, r = scan[0], g = scan[1], b = scan[2];
	      x < x_size; x++, r++, g++, b++) {
	      scan_line_buffer[x] = rmap[*r] + (gmap[*g] << 8) + (bmap[*b] << 16);
	 }	
	  /* XXX min ? */
	  ta_write2d(tahandle, scan_line_buffer, min(x_size, 1024), 1, 0, y_size - y);
     }

     /* be neat and free memory */
     free(scan_line_buffer);
     for (i = 0; i < 3; i++)
	  free(scan[i]);
}

static void
read_file_gray(hdr)
rle_hdr *hdr;
{

     unsigned char *scan[3];
     register unsigned char	*r, *g, *b;
     register int	x, value;
     unsigned char	*map;
     rle_pixel	**in_cmap;
     double	gamma;
     int	x_size, y_size, y;
     int i;


     RLE_CLR_BIT(*hdr, RLE_ALPHA);

     x_size = (hdr->xmax - hdr->xmin + 1);
     y_size = (hdr->ymax - hdr->ymin + 1);

     /* get memory for the scan line data */
     for (i = 0; i < 3; i++)
	  scan[i] = (unsigned char *) malloc(x_size);

     hdr->xmax -= hdr->xmin;
     hdr->xmin = 0;

     if (sscanf((char *)panel_get_value(gamma_item), "%lf", &gamma) != 1)
	  gamma = 1.0;

     in_cmap = buildmap( hdr, 1, 1.0 / gamma, 1.0 );

     map = &(in_cmap[0][0]);

     /* get memory for the output buffer */
     scan_line_buffer = (unsigned int *) malloc(x_size * sizeof(unsigned int));

     for ( i = 0; i < Y_PIXELS; i++)
	  ta_write2d(tahandle, erase, X_PIXELS, 1, 0, i);

     while ( (y = rle_getrow( hdr, scan )) <= hdr->ymax ) {
	  for(x = 0, r = scan[0], g = scan[1], b = scan[2];
	      x < x_size; x++, r++, g++, b++) {
	       value = (35*(*r)+55*(*g)+10*(*b)) / 100;
	       value = map[value];
	       scan_line_buffer[x] = value + (value << 8) + (value << 16);
	  }
	  /* XXX min ? */
	  ta_write2d(tahandle, scan_line_buffer, min(x_size, 1024), 1, 0, y_size - y);

     }

     /* be neat and free memory */
     free(scan_line_buffer);
     for (i = 0; i < 3; i++)
	  free(scan[i]);
}

static void
format_select(item, value, event)
Panel_item	item;
int	value;
Event	*event;
{
	if (value == 0)
		display_mode = RGB;
	else if (value == 1)
		display_mode = GRAY;
	else
		display_mode = RGB;
}



/*
 * the next three functions are from the sunview "touchup" editor
 * 
 * I have made a few changes to make the code work with gettaac
 * please forward bugs to me.  -ksp
 */

/**************************************************************************
   Touchup a bitmap graphics editor for the Sun Workstation running SunView
   Copyright (c) 1988 by Raymond Kreisel
   1/22/88 @ Suny Stony Brook

   This program may be redistributed without fee as long as this copyright
   notice is intact.

==> PLEASE send comments and bug reports to one of the following addresses:

	   Ray Kreisel
	   CS Dept., SUNY at Stony Brook, Stony Brook NY 11794

	   UUCP: {allegra, philabs, pyramid, research}!sbcs!rayk   
	   ARPA-Internet: rayk@sbcs.sunysb.edu			
	   CSnet: rayk@suny-sb
	   (If nobody is home at any of the above addresses try:
		S72QKRE@TOWSONVX.BITNET			        )

 "If I get home before daylight, I just might get some sleep tonight...."

**************************************************************************/
/**************************************************************************

/*
 * Let's do file completion on what we have in the file name prompt
 */
static void
make_new_name(item, event)
Panel_item      item;
Event           *event;
{
     static filename[MAX_FILE_NAME];

     {
	  strcpy(filename,(char*)panel_get_value(file_name));
	  if (complete(filename))
	       window_bell(panel);
	  panel_set(file_name,PANEL_VALUE,filename,0);
     }

}


/* This function, written by Marc J Newberger,
 * will do both login name completion and file name completion, DAM fast.
 * That means as fast as the csh does it.
 */
int complete(template)
char    *template;
{

     char    dirName[255];
     char   *prefix;    
     int     pref_len;
     char   *suffix;     
     char   *p, *q;
     char    first;
     char    nonUnique;
     char    twiddleUserCompletion;

     struct  direct     *nameEntry;
     DIR                *dirChan;
     struct  passwd     *pwdEntry;

     /*
      *  First do a little parsing of the input. Separate the
      *  prefix template from the directory if there is one.
      */
     twiddleUserCompletion= 0;
     prefix= template+strlen(template);
     while (*(--prefix) != '/' && prefix >= template);

     /*
      *  See if a directory was specified:
      */
     if (prefix < template) {
	  /*
	   *  No /'s, could be either a username completion or
	   *  a completion in the current directory.
	   */
	  if (template[0] == '~') {
	       prefix++;
	       twiddleUserCompletion= 1;
	  }
	  else {
	       strcpy(dirName, ".");
	  }
     }
     else if (prefix == template) {
	  /*
	   *  Special case !! The directory excluding the trailing
	   *  '/' is zero length. It's the root:
	   */
	  strcpy(dirName, "/");
     }
     else {
	  /*
	   *  We're completing a file in a directory.
	   *  The directory may be lead by a ~<username> abbreviation.
	   *  If that's the case expand it.
	   */
	  if (template[0] == '~') {
	       /*
		*  We need to do twiddle directory expansion.
		*  See if it's our directory:
		*/
	       if (template[1] == '/') {
		    strcpy(dirName, getenv("HOME"));
		    if ( &template[1] != prefix )
			 {
			      p= dirName+strlen(dirName);
			      q= &template[1];
			      while (q < prefix) {
				   *p= *q;
				   p++, q++;
			      }
			      *p= 0;
			 }
	       }
	       else {
		    /*
		     * It's someone else's. Let our fingers
		     * do the walking. (Why the fuck do they call it
		     * the "yellow pages" anyway. They're white pages
		     * dammit !  If they were YELLOW pages, we could
		     * say ypmatch "Automobile, Dealers, Retail", and
		     * things like that !).
		     */
		    for (p= dirName, q= &template[1];
			 (*p= *q) != '/';
			 p++, q++);
		    *p= 0;
		    if (!(pwdEntry= getpwnam(dirName))) {
			 return errno;
                    }
		    strcpy(dirName, pwdEntry->pw_dir);
		    p= dirName+strlen(dirName);
		    while (q < prefix) {
			 *p= *q;
			 p++, q++;
                    }
		    *p= 0;
	       }
	  }
	  else {
	       /*
		*  It's a vanilla directory. Strip it out.
		*/
	       strncpy(dirName, template, prefix-template);
	       dirName[prefix-template]= 0;
	  }
     }
     /*
      *  Bump prefix past the '/'.
      */
     prefix++;

     /*
      *  Get the prefix length and a pointer to the end of the
      *  prefix.
      */
     pref_len= strlen(prefix);
     suffix= template + strlen(template);

     /*
      *  See whether we're doing filename or username completion:
      */
     if (!twiddleUserCompletion) {

	  /*
	   *  It's filename completion. Read through the directory:
	   */
	  if ((dirChan= opendir(dirName)) == 0) {
	       return errno;
	  }

	  first= 1;
	  nonUnique= 0;
	  for (;;) {
	       if (!(nameEntry= readdir(dirChan))) {
		    break;
	       }
	       if (!strncmp(prefix, nameEntry->d_name, pref_len)) {
		    /*
		     *  We have a file that matches the template.
		     *  If it's the first one, we fill the completion
		     *  suffix with it. Otherwise we scan and pare down
		     *  the suffix.
		     */
		    if (first) {
			 first=  0 ;
			 strcpy(suffix, nameEntry->d_name+pref_len);
                    }
		    else {
			 nonUnique= 1;
			 p= suffix;
			 q= nameEntry->d_name+pref_len;
			 while (*p == *q) {
			      ++p; ++q;
			 }
			 *p= 0;

			 /*
			  *  A little optimization: If p == suffix, we
			  *  were unable to do any extension of the name.
			  *  We might as well quit here.
			  */
			 if (p == suffix) {
			      break;
			 }
                    }
	       }
	  }

	  closedir(dirChan);
     }
     else {
	  /*
	   *  Do ~Username completion. Start by resetting the passwd file.
	   */
	  setpwent();

	  first= 1;
	  nonUnique= 0;
	  for (;;) {
	       if (!(pwdEntry= getpwent())) {
		    break;
	       }
	       if (!strncmp(prefix, pwdEntry->pw_name, pref_len)) {
		    /*
		     *  We have a user that matches the template.
		     *  If it's the first one, we fill the completion
		     *  suffix with it. Otherwise we scan and pare down
		     *  the suffix.
		     */
		    if (first) {
			 first=  0 ;
			 strcpy(suffix, pwdEntry->pw_name+pref_len);
                    }
		    else {
			 p= suffix;
			 q= pwdEntry->pw_name+pref_len;
			 while (*p == *q) {
			      ++p; ++q;
			 }

			 /*
			  *  Here there is a possibility of seeing the
			  *  same username twice. For this reason, we
			  *  only set nonUnique to 1 if we're shortening
			  *  the suffix. This means that the new name is
			  *  distinct from any name we've seen.
			  */
			 if (*p) {
			      nonUnique= 1;
			      *p= 0;
			 }

			 /*
			  *  A little optimization: If p == suffix, we
			  *  were unable to do any extension of the name.
			  *  We might as well quit here.
			  */
			 if (p == suffix) {
			      break;
			 }
                    }
	       }
	  }
     }

     /*
      *  If nothing matched, return a -1, if there was non-uniqueness
      *  return -2.
      */ 
     if (first) {
	  return -1;
     }
     else if (nonUnique) {
	  return -2;
     }
     else {
	  return 0;
     }

}

/*
   * Take a filename with a ~ character at the begining and return
    * the full path name to that file
     */
get_full_path(template,full_path)
char template[];
char full_path[];
{
     char   *p, *q;
     struct  passwd     *pwdEntry;
     /*
      *  We're completing a file in a directory.
      *  The directory may be lead by a ~<username> abbreviation.
      *  If that's the case expand it.
      */
     if (template[0] == '~') {
	  /*
	   *  We need to do twiddle directory expansion.
	   *  See if it's our directory:
	   */
	  if (template[1] == '/') {
	       strcpy(full_path, getenv("HOME"));
	       strcat(full_path,&template[1]);
	  }
	  else {
						   
	       /*
		* It's someone else's. Let our fingers
		* do the walking. (Why the fuck do they call it
		* the "yellow pages" anyway. They're white pages
		* dammit !  If they were YELLOW pages, we could
		* say ypmatch "Automobile, Dealers, Retail", and
		* things like that !).
		*/
	       for (p= full_path, q= &template[1];
		    (*p= *q) != '/';
		    p++, q++);
	       *p= 0;
	       if (!(pwdEntry= getpwnam(full_path))) {
		    return errno;
	       }
	       strcpy(full_path, pwdEntry->pw_dir);
	       strcat(full_path,q);
	  }
     }
     else
	  strcpy(full_path,template);
}

/*
 * end of code from touchup
 */
