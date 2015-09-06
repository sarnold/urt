#import "Image.h"
#import <stdio.h>
#import <stdlib.h>
#import <strings.h>
#import <libc.h>
#import <zone.h>
#import <mach.h>
#import <streams/streams.h>
#import <sys/file.h>
#import <appkit/nextstd.h>
#import <appkit/NXBitmapImageRep.h>
#import <appkit/Window.h>
#import <appkit/Panel.h>
#import <appkit/graphics.h>
#import <appkit/tiff.h>

// Image.m 
// 
// Written by Vince DeMarco 
// 	demarco@cpsc.ucalgary.ca
//
// This program is In the Public Domain. If you make any improvements to this
// program please let me know
// 

@implementation Image:View

- init
{
    self = [super init];
    newZone = NXCreateZone(vm_page_size,vm_page_size,NO);
    numColors = 3;
    return(self);
}

- displayImage
{

    unsigned char *planes[4];

    NXSetRect(&temprect, 150.0, (800.0 - (double)ysize),(double)xsize, (double)ysize);

    thiswindow = [[Window allocFromZone:newZone]
		  initContent:&temprect
		  style:NX_TITLEDSTYLE
		  backing:NX_RETAINED
		  buttonMask:NX_CLOSEBUTTONMASK | NX_MINIATURIZEBUTTONMASK | NX_RESIZEBUTTONMASK
		  defer:NO];

    [thiswindow setDelegate:self];
    [thiswindow setFreeWhenClosed:YES];
    [thiswindow setTitleAsFilename:fileName];
    if (windowIconName[0]){
	[thiswindow setMiniwindowIcon:(const char *)windowIconName];
    }

    [thiswindow makeKeyAndOrderFront:nil];
    [thiswindow display];
    NXPing();

    if (r == NULL){     // Check if there is anything to display if there isn't
	return self;    // just return
    }

    planes[0] = r;
    planes[1] = g;
    planes[2] = b;
    planes[3] = a;

    tiffimage = [[NXBitmapImageRep allocFromZone:newZone]
		     initDataPlanes:  planes
		     pixelsWide:      xsize
		     pixelsHigh:      ysize
		     bitsPerSample:   8
		     samplesPerPixel: numColors
		     hasAlpha:        (numColors == 4 ? YES: NO )
		     isPlanar:        (numColors == 1 ? NO : YES)
		     colorSpace:      (numColors >= 3 ? NX_RGBColorSpace : NX_OneIsWhiteColorSpace)
		     bytesPerRow:     xsize*8
		     bitsPerPixel:    8];

    [thiswindow setContentView:self];
    [thiswindow display];
    return(self);
}

- drawSelf:(const NXRect *)rects :(int)rectCount
{
    [self getFrame:&temprect];
    temprect.origin.x = 0.0;
    temprect.origin.y = 0.0;

    [self lockFocus];
    [tiffimage drawIn:&temprect];
    [self unlockFocus];
    return self;
}

- free
{
    NX_FREE(a);
    NX_FREE(r);
    NX_FREE(g);
    NX_FREE(b);
    [super free];
    return(self);
}

- saveAsTiff:(const char *)filename usingCompression:(int)compression
{
    int fd;
    NXStream *stream;

    if ( (fd = open(filename,(O_RDWR|O_CREAT),0600)) < 0){
	return(self);
    }
    stream = NXOpenFile(fd,NX_READWRITE);
    [tiffimage writeTIFF:stream usingCompression:compression];
    NXClose(stream);
    close(fd);
    return(self);
}

- saveAsEPS:(const char *)filename
{
    int fd;
    NXStream *stream;

    if ( (fd = open(filename,(O_RDWR|O_CREAT),0600)) < 0){
	return(self);
    }
    stream = NXOpenFile(fd,NX_READWRITE);
    [self copyPSCodeInside:&temprect to:stream];
    NXClose(stream);
    close(fd);
    return(self);
}

// window Delegation methods
- windowWillClose:sender
{
    NX_FREE(a); // Free the alpha,red,green, and blue buffers
    NX_FREE(r);
    NX_FREE(g);
    NX_FREE(b);
    return(self);
}

@end
