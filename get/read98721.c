/* 
 * read98721 - read an RLE image from a display device
 * 
 * Author:	Filippo Tampieri
 * 		Program of Computer Graphics
 * 		Cornell University
 * Date:	Tue June 9 1987
 * 
 * [-m] - store maps
 * [-O] - store overlay (doesn't seem to do anything)
 * [-d display] - display device ("/dev/...")
 * [-x driver] - driver name ("hp98721")
 * [-p xstart ystart] - lower left pixel start
 * [-s xsize ysize] - size of image to store
 * [-b rbak gbak bbak] - background color
 * [-o fname] - output filename
 * [comments ...]
 * 
 */

#include "starbase.c.h"
#include <stdio.h>
#include "rle.h"

#define TRUE  1
#define FALSE 0
#define MAX(i,j)   ((i) > (j) ? (i) : (j))

#define NCHANS		3
#define NCMAPS		3
#define CMAPLEN		8
#define COLMAPLEN	256
#define MAX_X_SIZE	1280

/* if cmap_flag is TRUE the color maps are saved */
int cmap_flag = FALSE;

int display_flag = FALSE;
int driver_flag = FALSE;

/* default display device and device driver */
char *display_name = "/dev/crt";
char *driver_name  = "hp98721";

int pos_flag = FALSE;

/* (xstart,ystart) are the coordinates of the lower left corner of the block to be saved */
#define D_XSTART	0
#define D_YSTART	0
int xstart = D_XSTART;
int ystart = D_YSTART;

int size_flag = FALSE;

/* xsize and ysize are the size of the block to be saved */
#define D_XSIZE		512
#define D_YSIZE		480
int xsize = D_XSIZE;
int ysize = D_YSIZE;

int overlay_flag = FALSE;

/* if backgnd_flag is TRUE the background color given in backgnd[] is not explicitly saved */
int backgnd_flag = FALSE;

int backgnd[NCHANS];

int fname_flag = FALSE;

/* if fname is NULL the image is written to the standard output */
char *fname = NULL;

int ncomments = 0;
char **comments = NULL;

/* starbase file descriptor for device from which the picture is read */
int picture_fd;

main(argc, argv)
int	argc;
char	*argv[];
{
    int i,y;
    int dev_xmax,dev_ymax;
    rle_hdr the_hdr;
    rle_map cmap[COLMAPLEN * NCMAPS];
    unsigned char scanline[NCHANS][MAX_X_SIZE],*rows[NCHANS]; /* Scanline storage & RLE row pointers */

    if (scanargs(argc, argv,
    "% m%- O%- d%-display!s x%-driver!s p%-xstart!dystart!d s%-xsize!dysize!d \
	b%-rbak!dgbak!dbbak!d o%-fname!s comments%*s",
		 &cmap_flag,
		 &overlay_flag,
		 &display_flag,&display_name,
		 &driver_flag,&driver_name,
		 &pos_flag,&xstart,&ystart,
		 &size_flag,&xsize,&ysize,
		 &backgnd_flag,&(backgnd[0]),&(backgnd[1]),&(backgnd[2]),
		 &fname_flag,&fname,
		 &ncomments,&comments) == 0)
	exit(1);

    the_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &the_hdr, cmd_name( argv ), fname, 0 );

    if(overlay_flag && backgnd_flag) {
	fprintf(stderr,"read98721: incompatible options -O and -b\n");
	exit(1);
    }

    /* initialize the display device */
    setup_graphics_device(&dev_xmax,&dev_ymax);

    clip(dev_xmax,dev_ymax,&xstart,&ystart,&xsize,&ysize);
    if((xsize <= 0)||(ysize <= 0)) {
	fprintf(stderr,"%s: null image size\n", the_hdr.cmd);
	exit(1);
    }

    /* open output file */
    the_hdr.rle_file = rle_open_f(the_hdr.cmd, fname, "w");
    /* initialize image header */
    the_hdr.ncolors = NCHANS;
    the_hdr.bg_color = backgnd_flag ? backgnd : (int *)0;
    the_hdr.alpha = FALSE;
    the_hdr.background = backgnd_flag ? 2 : (overlay_flag ? 1 : 0);
    the_hdr.xmin = xstart;
    the_hdr.xmax = xstart + xsize -1;
    the_hdr.ymin = ystart;
    the_hdr.ymax = ystart + ysize -1;
    if(cmap_flag) {
        read_color_maps(picture_fd,cmap);
	the_hdr.ncmap = NCMAPS;
	the_hdr.cmaplen = CMAPLEN;
	the_hdr.cmap = cmap;
    } else {
	the_hdr.ncmap = 0;
	the_hdr.cmaplen = 0;
	the_hdr.cmap = (rle_map *)0;
    }
    the_hdr.bits[0] = 0x07;
    for(i=0; i<ncomments; i++)
	rle_putcom(comments[i],&the_hdr);

    rle_addhist( argv, (rle_hdr *)NULL, &the_hdr );
    rle_put_setup(&the_hdr);

    /* set up rows to point to our copy of the scanline */
    for (i= 0; i<NCHANS; i++)
	rows[i] = scanline[i];

    /* save image */
    for(y=the_hdr.ymin; y<=the_hdr.ymax; y++) {
	read_scanline(y,rows);
	rle_putrow(rows,xsize,&the_hdr);
    }

    rle_puteof(&the_hdr);

    exit(0);
}

/* initialize the device to 24 bit/pixel */
setup_graphics_device(dev_xmax,dev_ymax)
int *dev_xmax,*dev_ymax;
{
    	float p_lim[2][3],res[3],p1[3],p2[3];
    	int map_size;

    	picture_fd = gopen(display_name,OUTDEV,driver_name,0);
    	if(picture_fd < 0) {
		fprintf(stderr,"read98721: can't open device.\n");
		exit(1);
    	}
    	shade_mode(picture_fd,CMAP_FULL,FALSE);

    	/* find out info about this device. */
    	inquire_sizes(picture_fd,p_lim,res,p1,p2,&map_size);

    	/* set the screen limits in pixels so we can use starbase in a device
     	* independent way.
     	*
     	* it is assumed that the p_lim values returned by inquire_sizes are the
     	* lower left screen coordinates in (float) pixels and the upper right
     	* screen coordinates in (float) pixels.  It is also assumed that the
     	* range of values in x and y coordinates are given from 0 to some
     	* positive maximum value.
     	*/
    	*dev_xmax = MAX(p_lim[0][0],p_lim[1][0]) + 1.0;
    	*dev_ymax = MAX(p_lim[0][1],p_lim[1][1]) + 1.0;

    	/* set to use the whole display surface. */
    	mapping_mode(picture_fd,DISTORT);

    	/* set to address the pixels in a device independent way. */
    	vdc_extent(picture_fd,0.0,0.0,0.0,(float)(*dev_xmax)-1.0,
		   (float)(*dev_ymax)-1.0,0.0);
    	clip_rectangle(picture_fd,0.0,(float)(*dev_xmax)-1.0,
		       0.0,(float)(*dev_ymax)-1.0);
}

/* clip specified region size against maximum device window size */
clip(dev_xmax,dev_ymax,xstart,ystart,xsize,ysize)
int dev_xmax,dev_ymax,*xstart,*ystart,*xsize,*ysize;
{
	if(*xstart < 0) {
		*xsize += *xstart;
		*xstart = 0;
	}
	if(*ystart < 0) {
		*ysize += *ystart;
		*ystart = 0;
	}
	if(*xstart + *xsize > dev_xmax)
		*xsize = dev_xmax - *xstart;
	if(*ystart + *ysize > dev_ymax)
		*ysize = dev_ymax - *ystart;
}

/* read scanline y from the screen */
read_scanline(the_hdr,y,rows)
rle_hdr *the_hdr;
int y;
unsigned char *rows[];
{
	int i,j;

	for(i=0,j=NCHANS-1; i<NCHANS; i++,j--) {
		bank_switch(picture_fd,j,0);
    		block_read(picture_fd,(float)(the_hdr->xmin),(float)y,
		   the_hdr->xmax - the_hdr->xmin + 1,1,rows[i],FALSE);
	}
}

/* read the color maps */
read_color_maps(pic_fd,cmap)
int pic_fd;
rle_map *cmap;
{
	int i,cmaplen;
	rle_map *rmap,*gmap,*bmap;
	float colmap[COLMAPLEN][NCMAPS];

	inquire_color_table(pic_fd,0,COLMAPLEN,colmap);
	rmap = cmap;
	gmap = cmap + COLMAPLEN;
	bmap = cmap + COLMAPLEN * 2;
	for(i=0; i<COLMAPLEN; i++) {
		rmap[i] = colmap[i][2];
		gmap[i] = colmap[i][1];
		bmap[i] = colmap[i][0];
	}
}


