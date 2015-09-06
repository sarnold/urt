/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
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
/* gamma.c, 6/30/86, T. McCollough, UU */

#include <math.h>

#include "round.h"

int gamma ( x, gamma_value )
int x;
float gamma_value;
{
	static int gammamap[256];
	static float gv = 0.0;

	if (gv != gamma_value) {
		int i;

		gv = gamma_value;
		for (i = 0 ; i < 256 ; i++)
			gammamap[i] =
				round_positive(255.0 * pow( i/255.0, 1.0/gv));
	}

	return gammamap[x];
}
