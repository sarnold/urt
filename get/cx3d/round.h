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
/* round.h, 7/2/85, T. McCollough, UU */

/* need <math.h> */

#define round(x)		(int) floor((x)+0.5)

/* use round_positive() only if argument is positive */

#ifdef vax
				/* if we are on a vax, then
				make use of the fact that
				vaxen truncate when they
				convert from float (double)
				to int */

#define round_positive(x)	(int) ((x)+0.5)
#else
				/* we're not on a vax, so make
				no such assumption */

#define round_positive(x)	round(x)
#endif
