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
 * Author:	Martin R. Friedmann
 * 		Dept of Electrical Engineering and Computer Science
 *		University of Michigan
 * Date:	Tue, Nov 14, 1989
 * Copyright (c) 1989, University of Michigan
 */

#include "getx11.h"

/* this function provides a client side, block move within a single image
 * structure for panning around in the magnified image.  bcopy is probubly
 * faster on most machines...
 */

int
XCopyImage ( image, src_x, src_y, width, height, dst_x, dst_y )
XImage *image;
int src_x, src_y, width, height, dst_x, dst_y;
{
    int line_width = image->bytes_per_line;

    if ( width == 0 )
	return False;

    switch ( image->bits_per_pixel )
    {
    case 1:
	return False;

    case 8:
	do {
	    char *srcline, *dstline;
	    register char *src, *dst;
	    register int counter;

	    if ( src_y < dst_y )
	    {
		srcline = image->data + line_width * (src_y+height-1) +
		    src_x;
		dstline = image->data + line_width * (dst_y+height-1) +
		    dst_x;

		if ( src_x < dst_x ) {
		    dstline += width - 1;
		    while ( --height >= 0 ) {
			src = srcline + width - 1;
			dst = dstline;
			counter = width;

			duff8( counter, *dst-- = *src-- );

			srcline -= line_width;
			dstline -= line_width;
		    }
		}
		else {
		    srcline += width - 1;
		    while ( --height >= 0 ) {
			src = srcline - width + 1;
			dst = dstline;
			counter = width;

			duff8( counter, *dst++ = *src++ );

			srcline -= line_width;
			dstline -= line_width;
		    }
		}
	    }
	    else
	    {
		srcline = image->data + line_width * src_y + src_x;
		dstline = image->data + line_width * dst_y + dst_x;

		if ( src_x < dst_x ) {

		    dstline += width - 1;
		    while ( --height >= 0 ) {
			src = srcline + width - 1;
			dst = dstline;
			counter = width;

			duff8( counter, *dst-- = *src-- );

			srcline += line_width;
			dstline += line_width;
		    }
		}
		else {
		    srcline += width - 1;
		    while ( --height >= 0 ) {
			src = srcline - width + 1;
			dst = dstline;
			counter = width;

			duff8( counter, *dst++ = *src++ );

			srcline += line_width;
			dstline += line_width;
		    }
		}
	    }
	} while (0);
	return True;

    case 32:
	do {
	    register long *srcline, *dstline;
	    register long *src, *dst;

	    if ( src_y < dst_y )
	    {
		srcline = (long *) ((char *)image->data +
				    line_width * (src_y+height-1) +
				    src_x * sizeof(long));
		dstline = (long *) ((char *)image->data +
				    line_width * (dst_y+height-1) +
				    dst_x * sizeof(long));

		if ( src_x < dst_x ) {

		    dstline += width - 1;
		    while ( --height >= 0 ) {
			src = srcline + width - 1;
			dst = dstline;

			while ( src >= srcline )
			    *dst-- = *src--;

			srcline -= line_width/sizeof(long);
			dstline -= line_width/sizeof(long);
		    }
		}
		else {
		    srcline += width - 1;
		    while ( --height >= 0 ) {
			src = srcline - width + 1;
			dst = dstline;

			while ( src <= srcline )
			    *dst++ = *src++;

			srcline -= line_width/sizeof(long);
			dstline -= line_width/sizeof(long);
		    }
		}
	    }
	    else
	    {
		srcline = (long *) ((char *)image->data + line_width * src_y +
				    src_x * sizeof(long));
		dstline = (long *) ((char *)image->data + line_width * dst_y +
				    dst_x * sizeof(long));

		if ( src_x < dst_x ) {

		    dstline += width - 1;
		    while ( --height >= 0 ) {
			src = srcline + width - 1;
			dst = dstline;

			while ( src >= srcline )
			    *dst-- = *src--;

			srcline += line_width/sizeof(long);
			dstline += line_width/sizeof(long);
		    }
		}
		else {
		    srcline += width - 1;
		    while ( --height >= 0 ) {
			src = srcline - width + 1;
			dst = dstline;

			while ( src <= srcline )
			    *dst++ = *src++;

			srcline += line_width/sizeof(long);
			dstline += line_width/sizeof(long);
		    }
		}
	    }
	} while (0);
	return True;
    }
    return False;
}













