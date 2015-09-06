#ifndef lint
static char rcsid[] = "$Id: dvirle1.c,v 3.0.1.3 1992/02/28 22:12:53 spencer Exp $";
#endif

/*
 * DviRLE1 -- First half of DVI to RLE driver
 *
 * Reads DVI version 2 files and converts to an intermediate form that
 * is read by dvirle2.  Most of the work consists of converting DVI units
 * to RLE units, sorting pages, and rotating positions if the -h
 * flag is given.
 *
 * TODO:
 *	think about fonts with characters outside [0..127]
 */

/*
 * Converted from "verser1" written by Chris Torek.
 */


#include "rle_config.h"

#include <stdio.h>
#include "types.h"
#include "conv.h"
#include "dviclass.h"
#include "dvicodes.h"
#include "error.h"
#include "fio.h"
#include "font.h"
#include "gripes.h"
#include "dvistate.h"

#include "dvirle.h"

char	*ProgName;
extern char *optarg;
extern int optind;

/* Global variables. */
char	serrbuf[BUFSIZ];	/* buffer for stderr */

CONST_DECL char	*DVIFileName;	/* Name of input dvi file */

CONST_DECL char	*TeXFontDesc;	/* getenv(CONFENV): passed to dvirle2 */

/* arrays containing page info */
int	Chars;			/* number of chars {left,} on page */
int	MaxChars;		/* total space for chars */
int	*yx;			/* contains (y<<16|x) for each char */
int	*fcp;			/* contains (font<<14|char<<7|part) */
int	*nextyx;		/* pointer to next yx area */
int	*nextfcp;		/* pointer to next fcp area */

/*
 * A few experiments showed that the typical job uses less than 3200
 * characters per page.  This is an extensible array, so the initial value
 * affects only efficiency.
 */
#ifndef InitialChars
#define InitialChars 4000	/* initial number of chars to allocate */
#endif

/*
 * If there are many characters and rules that do not fit on a page,
 * these flags help reduce error output.
 */
int	TopEMsg;		/* true => gave error message about top */
int	BottomEMsg;		/* true => gave error message about bottom */
int	LeftEMsg;		/* true => gave error message about left */
int	RightEMsg;		/* true => gave error message about right */
int	RuleEMsg;		/* true => gave error message about rule */

int	CFlag;			/* -c => center output */
int	HFlag;			/* -h => horizontal (sideways) output */
int	SFlag;			/* -s => silent (no page #s) */
int	Debug;			/* -D => debug flag */

int	BottomMargin;		/* bottom margin (in pixels) */
int	TopMargin;		/* top margin (in pixels) */
int	WidestPageWidth;	/* width of widest page (in pixels) */
int	TallestPageHeight;	/* height of tallest page (in pixels) */
int	PageWidth;		/* Width of widest page + margins */
int	PageHeight;		/* Height of tallest page + margins */


/* Absolute value */
#define ABS(n) ((n) >= 0 ? (n) : -(n))


/*
 * Compute the pixel widths of the characters in the given font.
 */
void
ComputeCWidths(f)
	register struct font *f;
{
	register struct glyph *g;
	register int i;

	for (i = 0; i < 128; i++) {
		g = GLYPH(f, i);
		if (GVALID(g))
			g->g_pixwidth = fromSP(g->g_tfmwidth);
	}
}

/*
 * Have run out of room in the current yx and fcp arrays, so expand them.
 */
void
ExpandArrays()
{
	register unsigned newsize;

	MaxChars <<= 1;
	newsize = MaxChars * sizeof *yx;
	if ((yx = (int *) realloc((char *) yx, newsize)) == NULL)
		GripeOutOfMemory(newsize, "yx array");
	if ((fcp = (int *) realloc((char *) fcp, newsize)) == NULL)
		GripeOutOfMemory(newsize, "fcp array");
	Chars = MaxChars >> 1;
	nextyx = &yx[Chars];
	nextfcp = &fcp[Chars];
	--Chars;		/* we're about to use one */
}

/*
 * Sort the page arrays so that the values in yx are in ascending order.  We
 * use a Shell sort.
 */
void
SortPage()
{
	register int i, j, k, delta, *y, *f;

	/*
	 * Chars is currently the number of chars on the page, not the number
	 * of chars left in the array.
	 */
	y = yx;
	f = fcp;
	delta = 1;
	while (9 * delta + 4 < Chars)
		delta = 3 * delta + 1;
	while (delta > 0) {
		for (i = delta; i < Chars; i++) {
			if (y[j = i - delta] < y[i]) {
				register int t1 = y[i];
				register int t2 = f[i];

				k = i;
				do {
					y[k] = y[j];
					f[k] = f[j];
					k = j;
					j -= delta;
				} while (j >= 0 && y[j] < t1);
				y[k] = t1;
				f[k] = t2;
			}
		}
		delta /= 3;
	}
}

/*
 * Assign a unique number to each font in the DVI file.
 * Compute character widths for all the glyphs.
 */
struct font *
DefineFont(name, dvimag, dvidsz)
	char *name;
	i32 dvimag, dvidsz;
{
	register struct font *f;
	char *path;
	int len;
	static int next; /* dvirle2 knows that we use sequential indicies */

	if (next >= NFONTS)	/* fix this later */
		error(1, 0, "too many fonts (%d) used", next);

	f = GetRasterlessFont(name, dvimag, dvidsz, "versatec", &path);
	if (f == NULL) {
		GripeCannotGetFont(name, dvimag, dvidsz, "versatec", path);
		return (NULL);
	}
	if (Debug) {
		(void) fprintf(stderr, "[%s -> %s]\n", Font_TeXName(f), path);
		(void) fflush(stderr);
	}
	if (next == 0) {
		/*
		 * dvirle2 also needs the conversion factor,
		 * before the first font.
		 */
		PutLong(stdout, 300);	/* dots per inch */
		PutLong(stdout, ds.ds_usermag);
		PutLong(stdout, ds.ds_num);
		PutLong(stdout, ds.ds_denom);
		PutLong(stdout, ds.ds_dvimag);
	}
	putbyte(stdout, 1);	/* signal another font */
	PutLong(stdout, f->f_checksum);
	PutLong(stdout, dvimag);
	PutLong(stdout, dvidsz);
	len = strlen(name);
	PutLong(stdout, len);
	(void) fputs(name, stdout);
	ComputeCWidths(f);
	f->f_un.f_int = next++;
	return (f);
}

/*
 * Start a new page (interpreter found a DVI_BOP).
 */
void
BeginPage(count)
	i32 *count;
{

	if (!SFlag) {
		if (nextyx)
			(void) putc(' ', stderr);
		(void) fprintf(stderr, "[%d", (int)count[0]);
		(void) fflush(stderr);
	}

	/* Chars now becomes "number of characters left" */
	Chars = MaxChars;	/* empty the arrays */
	nextyx = yx;
	nextfcp = fcp;

	TopEMsg = 0;
	BottomEMsg = 0;
	LeftEMsg = 0;
	RightEMsg = 0;
	RuleEMsg = 0;
}

/*
 * End a page (process a DVI_EOP).
 */
void
EndPage()
{
	register int i, *y, *f, t, v, oldv;

	/* Chars now becomes "number of characters on page" */
	i = Chars = MaxChars - Chars;

	SortPage();

	if (!SFlag) {
		putc(']', stderr);
		(void) fflush(stderr);
	}
	y = yx;
	f = fcp;
	if ( HFlag )
	    oldv = PageWidth;
	else
	    oldv = PageHeight;
	while (--i >= 0) {
	    	if ( Debug > 1 )
		    fprintf( stderr, "oldv: %ld, yx: %lx (%d,%d), fcp: %lx\n",
			     oldv, *y, (*y >> 16), (*y & 0xffff), *f );
		v = *y >> 16;
		t = oldv - v;
		if (*f >= 0) {	/* setting a character */
			t = (t << 16) | (*y++ & 0xffff);
			PutLong(stdout, t);
			t = *f++;
			PutLong(stdout, t);	/* move down & place char */
		} else {	/* setting a rule */
			y++;
			if (t > 0) {	/* need to move down first */
				t = -t;
				PutLong(stdout, -1);
				PutLong(stdout, t);
			}
			t = *f++ & 0x7fffffff;
			PutLong(stdout, -1);
			PutLong(stdout, t);	/* place rule */
		}
		oldv = v;
	}

	/* Make all pages the same length */
	if (HFlag)
		t = -PageWidth;
	else
		t = -PageHeight;
	if (t) {
		PutLong(stdout, -1);	/* move down */
		PutLong(stdout, t);
	}
	PutLong(stdout, -1);
	PutLong(stdout, 0);	/* end of page */
}

/*
 * Perform a \special.
 * This version ignores all, with a warning.
 */
void
DoSpecial(len)
	i32 len;		/* length of the \special string */
{

	error(0, 0, "warning: ignoring \\special");
	(void) fseek(ds.ds_fp, (long) len, 1);
}


#ifndef lint
#define maxysize min(MaxCharHeight, 255)
#else
#define maxysize 255
#endif

/*
 * Set a rule at dvi_hh, dvi_vv, where h is the height of the rule
 * and w is the width (both in pixels).  (dvi_hh,dvi_vv) is the lower
 * left corner of the rule.
 */
void
SetRule(h, w)
	register int h, w;
{
	register int y;		/* temporary y value */
	register int x;		/* temporary x value */
	register int ymax;	/* bottommost (versatec-wise) y coord */
	register int xmax;	/* rightmost (versatec-wise) x coord */
	register int ymin;	/* topmost y coord */
	register int xmin;	/* leftmost x coord */
	int anybad = 0;

	if (!HFlag) {
		xmin = dvi_hh;
		ymax = dvi_vv;
		ymin = ymax - h;
		xmax = xmin + w;
	} else {
		ymin = dvi_hh;
		xmin = MaxPageWidth - dvi_vv - 1;/* ???DO I NEED -1 ANYMORE?? */
		xmax = xmin + h;
		ymax = ymin + w;
#ifdef notdef
		if (ymax > MaxPageHeight)
			ymax = MaxPageHeight, anybad++;
#endif
	}
	if (ymin < 0)
		ymin = 0, anybad++;
	if (xmin < 0)
		xmin = 0, anybad++;
	if (xmax > MaxPageWidth)
		xmax = MaxPageWidth, anybad++;
	if (anybad && !RuleEMsg) {
		error(0, 0, "WARNING: rule(s) off page edge; clipped to fit");
		RuleEMsg++;
	}
	for (y = ymax; y > ymin; y -= h) {
		for (x = xmin; x < xmax; x += w) {
			h = y - ymin;
			h = min(h, maxysize);
			w = xmax - x;
			w = min(w, 255);
			if (--Chars < 0)
				ExpandArrays();
			*nextyx++ = (y << 16) | x;
			*nextfcp++ = (1 << 31) | (x << 16) | (h << 8) | w;
		}
	}
}

/*
 * Check the range of a character, to be sure the device can handle it.
 * Called for DVI_SET and DVI_PUT opcodes only.
 */
int
CheckChar(c)
	i32 c;
{
	/* this driver needs work for codes > 127 */

	if ((ui32)c > 127) {
		error(0, 0, "Warning: character code %ld too big",
		    (long)c);
		return (1);
	}
	return (0);
}

/*
 * Main page loop.  This reads one page of the DVI file.
 * Returns 1 for EOP and 0 for end of last page (POST).
 */
int
PageLoop()
{
	static struct font NoFont;	/* font with zero pspace, etc */
	register int c;
	register i32 p = 0;
	register struct font *f = &NoFont;
	register FILE *fp = ds.ds_fp;
	int doingpage = 0, advance;

	static char warn[] = "\
WARNING: text object(s) run off %s of page; ignored";
#define CHECK(cond, msg, flag) \
	if (cond) { \
		if (!flag) { \
			error(0, 0, warn, msg); \
			flag = 1; \
		} \
		goto ignore; \
	}

	/*
	 * This would be a `for (;;)', but that makes the function
	 * crawl off the right of the screen.
	 *
	 * We handle ordinary characters early, as they are the
	 * most common opcodes in DVI files, and doing so makes the
	 * loop run faster.
	 */
loop:
	c = fgetbyte(fp);
	if (DVI_IsChar(c)) {
		register struct glyph *g;
		register int ulcx, ulcy, height;

		p = c;
		advance = 1;
do_char:
		g = GLYPH(f, p);
		if (!GVALID(g)) {
			GripeBadGlyph(p, f);
			goto loop;
		}
		if (!HFlag) {
			ulcy = dvi_vv + g->g_height - g->g_yorigin;
			ulcx = dvi_hh - g->g_xorigin;
			height = g->g_height;
			CHECK(ulcy < 0, "top", TopEMsg);
			CHECK(ulcx < 0, "left", LeftEMsg);
			CHECK(ulcx + g->g_width >= MaxPageWidth,
			    "right", RightEMsg);
		} else {/* rotate & translate */
			ulcy = dvi_hh + g->g_width - g->g_xorigin;
			ulcx = MaxPageWidth -
			    (dvi_vv + g->g_height - g->g_yorigin);
			height = g->g_width;
			CHECK(ulcy < 0, "left", LeftEMsg);
#ifdef notdef
			CHECK(ulcy + height >= MaxPageHeight,
			    "right", RightEMsg);
#endif
			CHECK(ulcx < 0, "bottom", BottomEMsg);
			CHECK(ulcx + g->g_height >= MaxPageWidth,
			    "top", TopEMsg);
		}
		for (c = 0; height > 0; c++) {
			if (--Chars < 0)
				ExpandArrays();
			*nextyx++ = (ulcy << 16) | ulcx;
			*nextfcp++ = (f->f_un.f_int << FONTSHIFT) |
				((p & CHARMASK) << CHARSHIFT) | c;
			height -= MaxCharHeight;
			ulcy += MaxCharHeight;
		}
ignore:
		if (advance) {
			dvi_h += g->g_tfmwidth;
			dvi_hh += g->g_pixwidth;
			p = fromSP(dvi_h);
			FIXDRIFT(dvi_hh, p);
		}
		goto loop;
	}

	if (c == EOF)		/* unexpected end of DVI file */
		GripeUnexpectedDVIEOF();

	/*
	 * Gather up a parameter, if known.
	 */
	switch (DVI_OpLen(c)) {

	case DPL_NONE:
		break;

	case DPL_SGN1:
		p = fgetbyte(fp);
		p = Sign8(p);
		break;

	case DPL_SGN2:
		fGetWord(fp, p);
		p = Sign16(p);
		break;

	case DPL_SGN3:
		fGet3Byte(fp, p);
		p = Sign24(p);
		break;

	case DPL_SGN4:
		fGetLong(fp, p);
		break;

	case DPL_UNS1:
		p = fgetbyte(fp);
		p = UnSign8(p);
		break;

	case DPL_UNS2:
		fGetWord(fp, p);
		p = UnSign16(p);
		break;

	case DPL_UNS3:
		fGet3Byte(fp, p);
		p = UnSign24(p);
		break;

	default:
		panic("DVI_OpLen(%d) = %d", c, DVI_OpLen(c));
		/* NOTREACHED */
	}

	/*
	 * Now switch on the type.
	 */
	switch (DVI_DT(c)) {

	case DT_SET:
		advance = 1;
		if (CheckChar(p))
			goto loop;
		goto do_char;

	case DT_PUT:
		advance = 0;
		if (CheckChar(p))
			goto loop;
		goto do_char;

	case DT_SETRULE:
		DVIRule(SetRule, 1);
		goto loop;

	case DT_PUTRULE:
		DVIRule(SetRule, 0);
		goto loop;

	case DT_NOP:
		goto loop;

	case DT_BOP:
		if (doingpage)
			GripeUnexpectedOp("BOP (already in page)");
		DVIBeginPage(BeginPage);
		doingpage = 1;
		goto loop;

	case DT_EOP:
		if (!doingpage)
			GripeUnexpectedOp("EOP (no BOP)");
		EndPage();
		return (1);

	case DT_PUSH:
		*ds.ds_sp++ = ds.ds_cur;
		goto loop;

	case DT_POP:
		ds.ds_cur = *--ds.ds_sp;
		goto loop;

	case DT_W0:
		p = dvi_w;
		goto right;

	case DT_W:
		dvi_w = p;
		goto right;

	case DT_X0:
		p = dvi_x;
		goto right;

	case DT_X:
		dvi_x = p;
		goto right;

	case DT_RIGHT:
right:
		dvi_h += p;
		if (F_SMALLH(f, p)) {
			dvi_hh += fromSP(p);
			p = fromSP(dvi_h);
			FIXDRIFT(dvi_hh, p);
		} else
			dvi_hh = fromSP(dvi_h);
		goto loop;

	case DT_Y0:
		p = dvi_y;
		goto down;

	case DT_Y:
		dvi_y = p;
		goto down;

	case DT_Z0:
		p = dvi_z;
		goto down;

	case DT_Z:
		dvi_z = p;
		goto down;

	case DT_DOWN:
down:
		dvi_v += p;
		if (F_SMALLV(f, p)) {
			dvi_vv += fromSP(p);
			p = fromSP(dvi_v);
			FIXDRIFT(dvi_vv, p);
		} else
			dvi_vv = fromSP(dvi_v);
		goto loop;

	case DT_FNTNUM:
		f = DVIFindFont((i32)(c - DVI_FNTNUM0));
		goto loop;

	case DT_FNT:
		f = DVIFindFont(p);
		goto loop;

	case DT_XXX:
		DoSpecial(p);
		goto loop;

	case DT_FNTDEF:
		SkipFontDef(fp);
		goto loop;

	case DT_PRE:
		GripeUnexpectedOp("PRE");
		/* NOTREACHED */

	case DT_POST:
		if (doingpage) {
			GripeUnexpectedOp("POST (no EOP)");
			/* NOTREACHED */
		}
		return (0);

	case DT_POSTPOST:
		GripeUnexpectedOp("POSTPOST");
		/* NOTREACHED */

	case DT_UNDEF:
		GripeUndefinedOp(c);
		/* NOTREACHED */

	default:
		panic("DVI_DT(%d) = %d", c, DVI_DT(c));
		/* NOTREACHED */
	}
	/* NOTREACHED */
}


void
main(argc, argv)
	int argc;
	register char **argv;
{
	register int c;
	register CONST_DECL char *s;
	int lmargin;
	FILE *fp = stdin;

	setbuf(stderr, serrbuf);

	ProgName = *argv;
	ds.ds_usermag = 1000;
	ds.ds_maxdrift = DefaultMaxDrift;
	DVIFileName = "`stdin'";

	while ((c = getopt(argc, argv, "cd:hm:sD")) != EOF) {
		switch (c) {

		case 'c':
			CFlag++;/* centered output */
			break;

		case 'd':	/* max drift value */
			ds.ds_maxdrift = atoi(optarg);
			break;

		case 'h':	/* horizontal output */
			HFlag++;
			break;

		case 'm':	/* magnification */
			ds.ds_usermag = atoi(optarg);
			break;

		case 's':	/* silent */
			SFlag++;
			break;

		case 'D':
			Debug++;
			break;

		case '?':
			(void) fprintf(stderr, "\
Usage: %s [-c] [-h] [-m mag] [-s] [file]\n", ProgName);
			(void) fflush(stderr);
			exit(1);
		}
	}

	if (optind < argc)
		if ((fp = fopen(DVIFileName = argv[optind], "r")) == NULL)
			error(1, -1, "can't open %s", argv[optind]);

#ifdef notdef
	if (MakeSeekable(stdin))
		error(1, 0,
			"unable to copy input to temp file (see the manual)");
#endif

	if ((TeXFontDesc = getenv(CONFENV)) == NULL)
		TeXFontDesc = "";

	c = (VERSION << 1) + (HFlag ? 1 : 0);
	putbyte(stdout, c);
	c = strlen(s = TeXFontDesc);
	PutLong(stdout, c);
	while (--c >= 0)
		putbyte(stdout, *s++);

	/* the margin offsets here are 0; margins computed later */
	DVISetState(fp, DefineFont, 300, 0, 0);
	putbyte(stdout, 0);	/* Signal end of fonts. */
	TallestPageHeight = fromSP(ds.ds_maxheight);
	WidestPageWidth = fromSP(ds.ds_maxwidth);

	/*
	 * The character plotter must check each character to ensure it
	 * is on the page, because the widest and tallest page values from
	 * the DVI file are not always accurate; so these tests do little
	 * save to keep one from finding the offending object.  Accordingly,
	 * I have disabled them.  20 May 1984 ACT.
	 */
#ifdef notdef
	if (HFlag) {
		if (MaxPageWidth - MinimumLeftMargin < TallestPageHeight)
			error(1, 0, "text object too high!");
		if (MaxPageHeight - MinimumTopMargin < WidestPageWidth)
			error(1, 0, "text object too wide!");
	} else {		/* page height can be safely ignored */
		if (MaxPageWidth - MinimumLeftMargin < WidestPageWidth)
			error(1, 0, "text object too wide!");
	}
#endif

	/* Determine margins */
	if (CFlag) {
		lmargin = (MaxPageWidth - WidestPageWidth) >> 1;
		if (lmargin < MinimumLeftMargin) {
			lmargin = MinimumLeftMargin;
			error(0, 0, "\
cannot center (page too wide); placing flush left instead");
		}
	} else
		lmargin = HFlag ? DefaultTopMargin : DefaultLeftMargin;

	if (HFlag) {
		TopMargin = (MaxPageHeight - WidestPageWidth) >> 1;
		if (TopMargin < 0)
			TopMargin = 0;
		BottomMargin = MaxPageHeight - TopMargin - WidestPageWidth;
		if (BottomMargin < 0)
			BottomMargin = 0;
	} else {
		TopMargin = DefaultTopMargin;
		BottomMargin = DefaultBottomMargin;
	}

	PageHeight = TallestPageHeight + BottomMargin + TopMargin;
	PageWidth = WidestPageWidth + BottomMargin + TopMargin;
	if ( HFlag )
	{
	    if ( PageHeight > MaxPageWidth )
	    {
		fprintf( stderr,
			 "Page too high (%d bits), truncated to %d bits\n",
			 PageHeight, MaxPageWidth );
		PageHeight = MaxPageWidth;
	    }
	    PutLong(stdout, PageWidth);
	    PutLong(stdout, PageHeight);
	}
	else
	{
	    if ( PageWidth > MaxPageWidth )
	    {
		fprintf( stderr,
			 "Page too wide (%d bits), truncated to %d bits\n",
			 PageWidth, MaxPageWidth );
		PageWidth = MaxPageWidth;
	    }
	    PutLong(stdout, PageHeight);
	    PutLong(stdout, PageWidth);
	}

	/* set the `fresh page' margins */
	ds.ds_fresh.hh = lmargin;
	ds.ds_fresh.vv = TopMargin;
	ds.ds_fresh.h = toSP(ds.ds_fresh.hh);
	ds.ds_fresh.v = toSP(ds.ds_fresh.vv);

	/* Allocate arrays */
	MaxChars = InitialChars;
	if ((yx = (int *) malloc(InitialChars * sizeof *yx)) == NULL)
		GripeOutOfMemory(InitialChars * sizeof *yx, "yx array");
	if ((fcp = (int *) malloc(InitialChars * sizeof *fcp)) == NULL)
		GripeOutOfMemory(InitialChars * sizeof *fcp, "fcp array");

	while (PageLoop())
		/* void */;

	exit(0);
	/* NOTREACHED */
}

/* Assume that if F_DUPFD is defined, dup2() is not.  Therefore, define it. */
#ifdef F_DUPFD
#include <fcntl.h>
dup2( fd1, fd2 )
int fd1, fd2;
{
    close(fd2);
    return fcntl( fd1, F_DUPFD, fd2 );
}
#endif
