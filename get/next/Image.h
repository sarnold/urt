#import <appkit/View.h>
#import <zone.h>
#import <streams/streams.h>

// Image.h
// 
// Written by Vince DeMarco 
// 	demarco@cpsc.ucalgary.ca
//
// This program is In the Public Domain. If you make any improvements to this
// program please let me know


@interface Image:View
{
    id                  thiswindow, thisview;
    id                  tiffimage;                // NXBitmapImageRep used to save the file
    int                 xsize, ysize;
    void               *rmap, *gmap, *bmap, *amap;
    unsigned char      *r,*g,*b,*a;
    NXZone             *newZone;
    char                fileName[1024];
    char                windowIconName[1024];
    int                 numColors;
    NXRect              temprect;	 // Temp place to store Window Rect
}

- init;
- displayImage;
- drawSelf:(const NXRect *)rects :(int)rectCount;
- free;
- saveAsTiff:(const char *)filename usingCompression:(int)compression;
- saveAsEPS:(const char *)filename;
- windowWillClose:sender;
@end

