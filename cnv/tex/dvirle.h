/* verser globals */

/*
 * Dvirle was rewritten based on a program called "verser".
 * The original program was written by Janet Incerpi of Brown University
 * and was for the original version of TeX which also used a different kind
 * of font file.  It was modified at the University of Washington by
 * Richard Furuta (bringing it up to TeX82 and PXL files) and Carl Binding
 * (adding horizontal printing).  I then tore it to shreds and rebuilt
 * it; the new one is much faster (though less portable:  it has inline
 * assembly code in various critical routines).
 *
 * Chris Torek, 20 May 1984, University of Maryland CS/EE
 *
 * The program has since gone through much revision.  The details are
 * rather boring, but there is one important point:  The intermediate
 * file format has changed.
 *
 * Converted to dvirle by Spencer W. Thomas, April 1987, U of Utah CS.
 */

/*
 * Version number.  Increment this iff the intermediate file format
 * makes an incompatible change.  This number may not be > 127.
 */
#define	VERSION	2

#define NFONTS	 100		/* max number of fonts */

#define FONTSHIFT 14		/* font shift in fcp's */
#define CHARSHIFT  7		/* char shift in fcp's */
#define CHARMASK 127		/* char mask in fcp's - 128 chars/font */
#define PARTMASK 127		/* part mask in fcp's */

#define	DPI 	300		/* Dots per inch */
#define	ROWS	600		/* lines in buffer (2 inches) */
#define	COLUMNS	319		/* 2550 bits per line / 8 bits per char */
#define MaxCharHeight (ROWS-1)	/* max bit height of a single char or rule */
#define MaxPageHeight  3300	/* max bit height of a page */
#define MaxPageWidth   2550	/* max bit width  of a page */

#define DefaultMaxDrift 2

#define DefaultLeftMargin   300
#define MinimumLeftMargin   15
#define DefaultTopMargin    300
#define MinimumTopMargin    15
#define DefaultBottomMargin 300

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
