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
 * in_cmap.c - Jack with the Input colormap from the rle files.
 * 
 * Author:	Martin R. Friedmann 
 * 		Dept of Electrical Engineering and Computer Science
 *		University of Michigan
 * Date:	Tue, April 12, 1990
 * Copyright (c) 1990, University of Michigan
 */

#include "getx11.h"

/* We really want this stuff to happen before find_appropriate_visual */
void check_mono_color( img, img_hdr )
image_information *img;
rle_hdr *img_hdr;
{
    /* Get some info out of the RLE header. */
    img->ncmap = img_hdr->ncmap;
    {
	char * v;
	if ( (v = rle_getcom( "color_map_length", img_hdr )) != NULL )
	    img->cmlen = atoi( v );
	else
	    img->cmlen = 1 << img_hdr->cmaplen;

	/* Protect against bogus information */
	if ( img->cmlen < 0 )
	    img->cmlen = 0;
	if ( img->cmlen > 256 )
	    img->cmlen = 256;
    }

    /* The mono_color flag means that a single input channel is being
     * pseudocolored by a multi-channel colormap.
     */
    img->mono_color = (img->img_channels == 1 && img->ncmap == 3 &&
		       img->cmlen);

}

/* uses global command line args (iflag, display_gamma) */
/* sets in_cmap, cmlen, ncmap and mono_color */

void get_dither_colors( img, img_hdr )
register image_information *img;
rle_hdr *img_hdr;
{
    register int i, j;

    /* Input map, at least 3 channels */
    if ( img->sep_colors ||
	 (img->img_channels == 1 && img->ncmap == 3 && img->cmlen) )
	/* If using color map directly, apply display gamma, too. */
	img->in_cmap = buildmap( img_hdr, 3, img->gamma,
				 display_gamma );
    else
	img->in_cmap = buildmap( img_hdr, 3, img->gamma, 1.0 );
    
    for (i = 0; i < 3; i++ ) {
	for (j = 0; j < 256; j++)
	    if ( img->in_cmap[i][j] != j )
		break;
	if (j < 256) break;
    }
    /* if i and j are maxed out, then in_cmap is the identity... */
    if ( i == 3 && j == 256 && !img->mono_color )
	img->in_cmap = NULL;
    else
	if ( debug_flag && !( i == 3 && j == 256) ) {
	    for (i = 0; i < 3; i++ ) {
		fprintf(stderr, "Input image colormap channel %d:\n", i);
		if ( i > 0 )
		    for ( j = 0; j < img->cmlen; j++ )
			if (img->in_cmap[i-1][j] != img->in_cmap[i][j])
			    break;
		if ( i > 0 && j == img->cmlen )
		    fprintf( stderr, "\tSame as channel %d\n", i - 1 );
		else
		    for (j = 0; j < img->cmlen; j += 16) {
			int k;
			fprintf (stderr, "%3d: ", j );
			for (k = 0; k < 16 ; k++ )
			    if ( j + k < img->cmlen )
				fprintf(stderr, "%3d ", img->in_cmap[i][j+k]);
			fprintf (stderr, "\n");
		    }
	    }
	}

    /* make colormap monochrome...   Whatahack! */
    if ( img->mono_color && !img->color_dpy ) {
	for (j = 0; j < 256; j++)
	    img->in_cmap[0][j] = (rle_pixel)
		((30 * img->in_cmap[0][j] + 59 * img->in_cmap[1][j] +
		  11 * img->in_cmap[2][j]) / 100);
	
	img->mono_color = False;
    }
}    

int eq_cmap( cm1, len1, cm2, len2 )
register rle_pixel **cm1, **cm2;
int len1, len2;
{
    register int i, j;
    
    if (cm1 && cm2) {
	if ( len1 != len2 )
	    return 0;
	for (i = 0; i < 3; i++ ) 
	    for (j = 0; j < len1; j++)
		if ( cm1[i][j] != cm2[i][j] )
		    return 0;
    } else
	if (cm1 || cm2) 
	    return 0;
    return 1;
}
