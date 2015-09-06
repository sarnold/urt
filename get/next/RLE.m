#import "RLE.h"
#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <libc.h>
#import <zone.h>
#import <mach.h>
#import <appkit/Window.h>
#import <appkit/Panel.h>
#import <appkit/tiff.h>

// RLE.m 
// 
// Written by Vince DeMarco 
// 	demarco@cpsc.ucalgary.ca
//
// This program is In the Public Domain. If you make any improvements to this
// program please let me know
// 

@implementation RLE:Image

- init
{
    self = [super init];
    strncpy(windowIconName,"rle.icon.tiff",14);
    rle_file_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    return(self);
}

- open:(char *)filename
{
    int                 i,j,row_pos;

    rle_pixel  **scan;

    thisview = self;
    rmap = 0;

    strncpy(fileName,filename,1024);
    rle_file_hdr.rle_file = rle_open_f_noexit("RLEViewer", fileName, "rb");
    rle_names( &rle_file_hdr, "RLEViewer", filename, 0 );

    if (rle_file_hdr.rle_file == NULL) {
	NXRunAlertPanel("RLE ERROR", "Can't open %s", NULL, NULL, NULL, fileName);
	return(self);
    }


    //Read in the header from the RLE file
    if(rle_get_setup(&rle_file_hdr) != RLE_SUCCESS){
	NXRunAlertPanel("RLE ERROR", "%s Not an RLE file ", NULL, NULL, NULL, fileName);
	return(self);
    }

    xsize = rle_file_hdr.xmax - rle_file_hdr.xmin + 1;
    ysize = rle_file_hdr.ymax - rle_file_hdr.ymin + 1;

    rle_file_hdr.xmax -= rle_file_hdr.xmin;
    rle_file_hdr.xmin = 0;
    numColors = rle_file_hdr.ncolors;

    if (rle_file_hdr.alpha && (numColors == 3))
	numColors++;

    if (rle_row_alloc(&rle_file_hdr,&scan) < 0){
	NXRunAlertPanel("RLE ERROR", "%s Malloc Error ", NULL, NULL, NULL, fileName);
	return(self);
    }

    // Read each scan line into memory ready for displaying. 
    if (numColors == 1){
	r = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));
        for (i = ysize; i > 0; i--) {

		rle_getrow(&rle_file_hdr, scan);
		j = i*xsize;
		row_pos = 0;

		while(j <= (i*xsize)+xsize){   // Copy each scanline into a temp
		    r[j]=scan[0][row_pos];     // dataspace
		    j++;
		    row_pos++;
		  }
	    }
    }
    if (numColors == 3){
	r = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));
	g = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));
	b = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));
        for (i = ysize; i > 0; i--) {

		rle_getrow(&rle_file_hdr, scan);
		j = i*xsize;
		row_pos = 0;

		while(j <= (i*xsize)+xsize){   // Copy each scanline into a temp
		    r[j]=scan[0][row_pos];     // dataspace
		    g[j]=scan[1][row_pos];
		    b[j]=scan[2][row_pos];
		    j++;
		    row_pos++;
		  }
	    }
    }

    if (numColors == 4){
	r = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));
	g = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));
	b = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));
	a = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));

        for (i = ysize; i > 0; i--) {

		rle_getrow(&rle_file_hdr, scan);
		j = i*xsize;
		row_pos = 0;

		while(j <= (i*xsize)+xsize){   // Copy each scanline into a temp
		    r[j]=scan[0] [row_pos];    // dataspace
		    g[j]=scan[1] [row_pos];
		    b[j]=scan[2] [row_pos];
                    a[j]=scan[-1][row_pos];
		    j++;
		    row_pos++;
		  }
	    }
    }

    // Program WILL DIE if you have a file with a valid rle header but
    // nothing for it to read, it will read the NOTHING in but when you
    // try to render it, the program will die.
    
    rle_row_free(&rle_file_hdr,scan);
    fclose(rle_file_hdr.rle_file);
    [self displayImage];
    return(self);
}

@end
