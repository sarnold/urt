#import "RASF.h"
#import <stdio.h>
#import <stdlib.h>
#import <libc.h>
#import <zone.h>
#import <mach.h>
#import <appkit/Window.h>
#import <appkit/Panel.h>
#import <appkit/tiff.h>

// RASF.m 
// 
// Written by Vince DeMarco 
// 	demarco@cpsc.ucalgary.ca
//
// This program is In the Public Domain. If you make any improvements to this
// program please let me know
// 

@implementation RASF:Image

- init
{
    self = [super init];
    strncpy(windowIconName,"rasf.icon.tiff",14);
    return(self);
}

// The open method written below is 
// Based on code Written By Thomas Orth 
//                            orth@cpsc.ucalgary.ca
//
//  Acually almost completely copied from Tom, I just changed a few variables
//  and other meaningless stuff like that.
//
-open:(char *)filename
{
    NXStream *fp;
    int x,y;
    int i;
    unsigned char red,green,blue,n;
    unsigned char buf[80];
    int count = 0;

    thisview = self;
    rmap = 0;
    strncpy(fileName,filename,1024);
    if ((fp = NXMapFile(fileName,NX_READONLY)) == NULL) {
	NXRunAlertPanel("RASF ERROR", "Can't open %s", NULL, NULL, NULL, fileName);
	return(self);
    }

    //Read in the header from the RASF file
    NXRead(fp,buf,sizeof(char)*8);
    if(strncmp((char *)buf,"gl  RASF",8)){
	NXRunAlertPanel("RASF ERROR", "%s Not an RASF file ", NULL, NULL, NULL, fileName);
	return(self);
    }

    // Read in the size of the raster file from the header
    for (i=0;i<2;i++){
	NXRead(fp,&red,sizeof(char));
    }
    NXRead(fp,&red,sizeof(char));
    NXRead(fp,&green,sizeof(char));
    xsize=red*256+green;

    for (i=0;i<2;i++){
	NXRead(fp,&red,sizeof(char));
    }
    NXRead(fp,&red,sizeof(char));
    NXRead(fp,&green,sizeof(char));
    ysize=red*256+green;

    // Now comes the erase values which we will just ignore
    for (i=0;i<64;i++){
	NXRead(fp,&red,sizeof(char));
    }

    // Allocate three buffers to store the R,G,B Triplets
    r = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));
    g = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));
    b = (unsigned char *)NXZoneMalloc(newZone,ysize*xsize*(sizeof(unsigned char)));

    i = 0;
    for (y=(ysize-1);y>=0;y--){
	for (x=0;x<xsize;x++){
	    if (count == 0){
		NXRead(fp,&red,sizeof(char));
		NXRead(fp,&green,sizeof(char));
		NXRead(fp,&blue,sizeof(char));
		NXRead(fp,&n,sizeof(char));
		r[i]  = red;
		g[i]  = green;
		b[i]  = blue;
		i++;
		count=n;
	    }else{
		count--;
		r[i]  = red;
		g[i]  = green;
		b[i]  = blue;
		i++;
	    }
	}
    }

    numColors = 3;
    [self displayImage];
    NXCloseMemory(fp,NX_FREEBUFFER);
    return(self);
}

@end
