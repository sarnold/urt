/* 
 * wedge.c - Step wedge for the QCR-Z
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Jan 14 1988
 * Copyright (c) 1988, University of Utah
 */

#include <stdio.h>
#include "rle.h"
#include "qcr.h"

#define IMG_WIDTH 2048
#define IMG_HEIGHT 1536

/* 
 * The "+1" is so that we get the full dynamic range, 0..255.  If we want
 * 16 levels, this requires 17 steps to include the zero and full intensity.
 */
#define STEPS (16+1)

rle_pixel * raster;

static char * color_names[3] = { "red", "green", "blue" };

main()
{
    register rle_pixel * rptr;
    register int i,j,color, val;

    raster = (rle_pixel *) malloc( (STEPS * 2 + 2) * IMG_HEIGHT /*/4*/ );
    rptr = raster;

    init_qcr( 1 );

    for (color = 0; color < 3; color++)
	for (j = 0; j < IMG_HEIGHT/4; j++)
	{
	    for (i = 0; i < STEPS; i++)
	    {
		*rptr = IMG_WIDTH / STEPS;
		val = i * (256 / (STEPS-1));
		if (val >= 256)
		{
		    val = 255;
		    /* Add round-off to fill out scanline */
		    *rptr += IMG_WIDTH - (STEPS * (IMG_WIDTH / STEPS));
		}
		rptr++;
		*rptr++ = val;
	    }
	}

    /* Do first three color bars. */
    for (color = 0; color < 3; color++)
    {
	set_up_qcr( IMG_WIDTH, IMG_HEIGHT, IMG_HEIGHT/4,
		   color * (IMG_HEIGHT/4) );

	fprintf(stderr, "Sending color %s...\n", color_names[color] );
	write_qcr_cmd( RED_RLE + color );

	if (write_data( raster, (STEPS * 2) * (IMG_HEIGHT / 4) ) <= 0)
	    perror("wedge: error sending data");
	qcr_wait_srq();
	fprintf(stderr, "...sent\n\n");
    }

    color = 3;
    set_up_qcr( IMG_WIDTH, IMG_HEIGHT, IMG_HEIGHT/4,
	       color * (IMG_HEIGHT/4) );

    fprintf(stderr, "Sending three passes (for gray)...\n");
    write_qcr_cmd( THREE_PASS_RLE );

    if (write_data( raster, ((STEPS * 2) * (IMG_HEIGHT / 4)) * 3 ) <= 0)
	perror("wedge: error sending data");
    qcr_wait_srq();
    fprintf(stderr, "...sent\n\n");
}
