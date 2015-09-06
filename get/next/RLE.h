#import "Image.h"
#import <rle.h>

// RLE.h
// 
// Written by Vince DeMarco 
// 	demarco@cpsc.ucalgary.ca
//
// This program is In the Public Domain. If you make any improvements to this
// program please let me know


@interface RLE:Image
{
    struct rle_hdr rle_file_hdr;
}
- init;
- open:(char *)filename;
@end

