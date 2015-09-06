#ifndef lint
static char rcsid[] = "$Id: dvirle2.c,v 3.0.1.1 1992/01/21 19:26:59 spencer Exp $";
#endif

/*
 * Dvirle2 -- Second half of DVI to RLE converter
 *
 * Reads pre-sorted pages as put out by dvirle1, and shovels bitmaps
 * out to the RLE file as fast as possible.  Warning:  there is some
 * inline assembly code used in inner loops, where the C compiler
 * produced particuarly poor code.
 *
 * We use a technique known as a `band buffer', where we keep track
 * of what has yet to be written to the RLE file in a buffer that
 * represents a `band' across the current page, analagous to a magnifying
 * bar across the page.  Only the region in the band can be written,
 * and the band moves only downward; this is why dvirle1 must sort
 * each page, at least by y coordinate.  This also implies that the
 * `tallest' object we can write is the same height as the band.  This
 * is a problem for large characters.  For these there is some (as yet
 * unimplemented) code that will ask for a `part' of each character
 * to be drawn into the band.  The character would then be repeated
 * with a request for the next part when the band has moved down to
 * just below the bottom of the previous part.  Rules are also broken
 * up as appropriate (and that code *is* implemented).
 *
 * Another important point is that the band buffer is treated as a
 * `cylinder' rather than a `strip': we write bits onto the cylinder,
 * then roll it forward over the page, moving the bits off the cylinder
 * and onto the paper, leaving that part of the cylinder clean, ready
 * for more bits.  The variable `CurRow' points at the current row
 * in the buffer/on the cylinder, and `FirstRow' and `LastRow' bound
 * the `dirty' part of the cylinder.  Modular arithmetic suffices to
 * change linear to cylindrical.
 *
 * Yet another point of note is that because the band always moves
 * `down' on the page, we need only a positive offset from the current
 * row to move to a new row.  This means (among other things) that we
 * can use negative offsets for special purposes.
 */

#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include "types.h"
#include "conv.h"
#include "fio.h"
#include "font.h"
#include "dvirle.h"
#include "rle.h"

/*#define	SPEED_HACK*/

void ReadFonts(), FormFeed(), ReadInput(), VWriteChar(), VWriteRule();
void DumpTopOfBand(), MoveDown(), WriteBuf(), WriteBlanks();

char	*ProgName;
extern int errno;
extern char *optarg;
extern int optind;

/* Global variables. */
struct font *Fonts[NFONTS];	/* the fonts */

char	TeXFontDesc[256];	/* getenv("TEXFONTDESC") from dvirle1 */

int	RasterOrientation;	/* ROT_NORM or ROT_RIGHT, based on HFlag */

int	DFlag;			/* -d => output discarded */
int	HFlag;			/* -h => horizontal (rotated bitmaps) */
int	SFlag;			/* -s => silent processing */
int	TFlag;			/* -t => output to tape */
int	Debug;			/* -D => debug flag */

unsigned char	VBuffer[ROWS][COLUMNS]; /* band buffer */

int 	NewPage;		/* New page indicator */

int	CurRow;			/* current row in buffer */
int	CurCol;			/* current column in buffer */
int	FirstRow;		/* the first row used */
int	LastRow;		/* the last row used */
int	NLines;			/* counts lines; used for pagefeeds */
int	PageHeight, PageWidth;	/* Page size, from pass 1 */

int 	filtw = 5, filth = 5;	/* Filter dimensions */
int 	filtfact;		/* multiplier for filter averaging */
int 	outwidth;		/* width of output line */

rle_hdr	out_hdr;

CONST_DECL char *  pic_comments[] = {
    "Creator=DVIRLE",
    "Note=Created from DVI file at 300 dots/inch",
    0
};

/*
 * RowsBetween tells how many rows (in cylindrical arithmetic) there
 * are between the first position and the second.  If the second value
 * is less than the first value, add ROWS to do the appropriate modular
 * arithmetic.  We cannot use `%' as C `%' is machine-dependent with
 * respect to negative values.
 */
#define RowsBetween(f, n) ((n) >= (f) ? (n) - (f) : (n) - (f) + ROWS)

/*
 * This is it... on your marks ... get set ... main!
 */
void
main(argc, argv)
	int argc;
	register char **argv;
{
	register int c;
	register char *s;
	int dpi, usermag, num, denom, dvimag;

	ProgName = *argv;

	while ((c = getopt(argc, argv, "dstDx:y:")) != EOF) {
		switch (c) {

		case 'd':	/* output to /dev/null */
			DFlag++;
			break;

		case 's':	/* silent processing except for errors */
			SFlag++;
			break;

		case 't':	/* output to tape (not implemented) */
			TFlag++;
			error(0, 0, "tape option not yet implemented");
			break;

		case 'D':
			Debug++;
			break;

		case 'x':
			if ( optarg )
			    filtw = atoi( optarg );
			break;
		case 'y':
			if ( optarg )
			    filth = atoi( optarg );
			break;
		case '?':
			fprintf(stderr, "Usage: %s [-d] [-s] [-t] [file]\n",
				ProgName);
			exit(1);
		}
	}
	if (optind < argc)
		if (freopen(argv[optind], "r", stdin) == NULL)
			error(1, 0, "can't open %s", argv[optind]);

	HFlag = getchar();
	if ((HFlag >> 1) != VERSION)
		error(1, 0, "input file is not version %d", VERSION);
	HFlag &= 1;
	RasterOrientation = HFlag ? ROT_RIGHT : ROT_NORM;

	s = TeXFontDesc;
	c = GetLong(stdin);
	while (--c >= 0)
		*s++ = getchar();
	if (feof(stdin))
		(void) GetByte(stdin);	/* let GetByte do error */
	*s = 0;

	dpi = GetLong(stdin);
	usermag = GetLong(stdin);
	num = GetLong(stdin);
	denom = GetLong(stdin);
	dvimag = GetLong(stdin);
	SetConversion(dpi, usermag, num, denom, dvimag);

	fontinit(*TeXFontDesc ? TeXFontDesc : (char *) NULL);
	ReadFonts();

	PageHeight = GetLong(stdin);
	PageWidth = GetLong(stdin);

	out_hdr = *rle_hdr_init( (rle_hdr *)NULL );
	rle_names( &out_hdr, cmd_name( argv ), NULL, 0 );

	if (DFlag) {
		(void) fprintf(stderr, "Output will be discarded\n");
		(void) fflush(stderr);
		out_hdr.rle_file = fopen("/dev/null", "w");
	}

	/* Set up output file papameters */
	out_hdr.xmin = 0;
	out_hdr.xmax = outwidth = (PageWidth - 1) / filtw;
	out_hdr.ymin = 0;
	out_hdr.ymax = (PageHeight - 1) / filth;
	out_hdr.ncolors = 1;
	out_hdr.alpha = 1;
	RLE_SET_BIT( out_hdr, RLE_ALPHA );
	out_hdr.comments = pic_comments;
	rle_put_setup( &out_hdr );

	filtfact = 255.0 / (filtw * filth);

	ReadInput();

	FormFeed();		/* Write last EOF */

	if (!SFlag)
		(void) putc('\n', stderr);

	exit(0);
	/* NOTREACHED */
}

/*
 * Read the font definitions.
 *
 * Anti-streak hack: get the rasters ahead of time, #ifdef SPEED_HACK.
 */
void
ReadFonts()
{
	register struct font *f, **fp;
	register int c;
	register char *s;
#ifdef SPEED_HACK
	register struct glyph *g;
#endif
	i32 mag, dsize;
	char *fname;
	char nm[512];

	if (!SFlag)
		(void) fprintf(stderr, "[fonts:\n");
	fp = Fonts;
	while (GetByte(stdin) == 1) {
		(void) GetLong(stdin);	/* checksum */
		mag = GetLong(stdin);	/* magfactor */
		dsize = GetLong(stdin);	/* design size */
		c = GetLong(stdin);
		s = nm;
		while (--c >= 0)
			*s++ = getchar();
		if (feof(stdin))
			(void) GetByte(stdin);	/* let GetByte do error */
		*s = 0;
		f = GetFont(nm, mag, dsize, "RLE", &fname);
		if (f == NULL) {
			GripeCannotGetFont(nm, mag, dsize, "RLE", fname);
			exit(1);
			/* NOTREACHED */
		}
		if (Debug) {
			(void) fprintf(stderr, "[%s -> %s]\n",
				Font_TeXName(f), fname);
			(void) fflush(stderr);
		}
		if (!SFlag) {
			register char *t = fname;

			s = fname;
			while (*s)
				if (*s++ == '/' && *s)
					t = s;
			(void) fprintf(stderr, " %s\n", t);
		}
#ifdef SPEED_HACK
		for (c = 0; c < 128; c++) {
			g = GLYPH(f, c);
			if (GVALID(g))
				(void) RASTER(g, f, RasterOrientation);
		}
#endif
		*fp++ = f;
	}
	if (!SFlag)
		(void) fprintf(stderr, "]\n");
}

/*
 * Read the input stream, decode it, and put character rasters or rules at
 * the positions given.
 */
void
ReadInput()
{
	register int yx, fcp, height;

	/*
	 * Loop forever.  I had a `for (;;)' but everything crept off the
	 * right side of the screen. 
	 */
next:
	fGetLong(stdin, yx);	/* position */
	fGetLong(stdin, fcp);	/* character, most likely */
	if ( Debug > 2 )
	    fprintf( stderr, "yx:%lx, fcp:%lx\n", yx, fcp );
	if (feof(stdin))
		return;		/* done */

	/*
	 * A `position' of -1 indicates either a rule or an end of page.
	 * Anything else is a character.
	 */
	if (yx != -1) {		/* place character */
		register struct glyph *g;
		register struct font *f;
		register int fnum;

		/*
		 * Any delta-y required is stored in the upper 16 bits of yx.
		 */
		if ((height = yx >> 16) != 0)
			MoveDown(height);
		/*
		 * Extract the x, font, char, and part info into CurCol,
		 * fnum, yx, and fcp.
		 */
		CurCol = yx & 0xffff;
		fnum = fcp >> FONTSHIFT;
		yx = (fcp >> CHARSHIFT) & CHARMASK;
		fcp = fcp & PARTMASK;
		f = Fonts[fnum];	/* trusting */
		g = GLYPH(f, yx);
		if ( Debug > 1 )
		    fprintf( stderr,
			     "Col:%d, Height:%d, fnum:%d, yx:%d, fcp:%d\n",
			     CurCol, height, fnum, yx, fcp );

		/*
		 * In case this character does not fit, write
		 * out the used part of the band.  It had better
		 * fit afterward....
		 */
		height = g->g_height;
		if (height >= ROWS - RowsBetween(FirstRow, CurRow))
			DumpTopOfBand();
		if (fcp)	/* cannot handle these yet */
			error(0, 0, "\
part code not implemented; skipping char %d in %s",
				yx, f->f_path);
		else if (HASRASTER(g)) {
#ifdef SPEED_HACK
			/* XXX, but saves time */
			VWriteChar(g->g_raster, height, g->g_width);
#else
			VWriteChar(RASTER(g, f, RasterOrientation),
				height, g->g_width);
#endif
		}
		goto next;	/* done with character */
	}

	/*
	 * If the `character' is negative, we need to move down first,
	 * possibly because this is an end-of-page.  If this is not the
	 * end of the page, it must be a rule.
	 */
	if (fcp < 0) {		/* move down */
		yx = -fcp;
		fGetLong(stdin, fcp);	/* junk */
		fGetLong(stdin, fcp);
		if (fcp == 0) {	/* end page */
		    	if ( Debug > 1 )
				fprintf( stderr, "End of Page\n" );
			/* dump entire band */
			WriteBuf(&VBuffer[0][0], FirstRow, LastRow, 1);
			CurRow = LastRow = FirstRow;
			if (!HFlag) {
				WriteBlanks(yx - NLines);
				FormFeed();
			} else
				FormFeed();
			if (!SFlag)
				(void) fprintf(stderr, ".");
			NLines = 0;
			goto next;	/* all done */
		}

		MoveDown(yx);	/* must be a rule; move down by yx rows */
	}

	/*
	 * At this point we have a rule to put at the current
	 * position, CurRow.
	 */
	height = (fcp & 0xff00) >> 8;
	/* make sure it fits */
	if (height >= ROWS - RowsBetween(FirstRow, CurRow))
		DumpTopOfBand();
	VWriteRule(fcp);
	goto next;		/* done with rule */
}

/*
 * Write the given raster for the given character.
 *
 * Basically, the task is to move bits from the raster to the output
 * buffer.  However, because the character being plotted can be on an
 * arbitrary bit boundary, things are not as simple as we might like.
 * The solution used here is to shift each raster value right, OR it
 * into the buffer, then (at the next location) OR in the bits that
 * `fell off the right edge'.
 */
void
VWriteChar(rastp, height, width)
	unsigned char *rastp;		/* raster pointer */
	int height, width;	/* height & width of char */
{
	register unsigned char *bp;	/* Output buffer pointer [r11] */
	register unsigned char *rp;	/* raster pointer	   [r10] */
	register int rshift;	/* right shift index	   [r9]  */
	register int lshift;	/* left shift index	   [r8]  */
	register int j;		/* width loop downcounter */
	register int o;		/* offset to next row in buffer */
	int row;		/* current row in buffer */
	int col;		/* column in buffer of left edge */
	int i;			/* height loop downcounter */
	int w;			/* raster width (bytes) */

	if ((rp = rastp) == NULL)
		return;		/* an all-white character */

	row = CurRow + height - 1;
	if ( row >= ROWS )
	    row -= ROWS;	/* assume height <= ROWS? */
	col = CurCol >> 3;
	i = height;
	w = (width + 7) >> 3;
	o = COLUMNS + w;

#if defined(lint) || !defined(vax)
	rshift = CurCol & 7;
	lshift = 8 - rshift;
#else /* lint || !vax */
	rshift = -(CurCol & 7);	/* Vax does '>>' as negative '<<' */
	lshift = 8 + rshift;
#endif /* lint || !vax */
	bp = &VBuffer[row][col];

#define avoiding_shifts_is_faster	/* but is it??? */
#ifdef avoiding_shifts_is_faster
	/*
	 * One out of eight or so times, the shift values will be
	 * zero.  This makes the code run faster.
	 */
	if (rshift == 0) {
		while (--i >= 0) {
			j = w;
			while (--j >= 0)
				*bp++ |= *rp++;
			if (--row < 0) {
				row = ROWS - 1;
				bp = &VBuffer[ROWS - 1][col];
			} else
				bp -= o;
		}
	} else
#endif
	{
		while (--i >= 0) {
			j = w;
			while (--j >= 0) {
#if defined(lint) || !defined(vax)
				*bp++ |= (*rp & 255) >> rshift;
				*bp |= (*rp++ & 255) << lshift;
#else /* lint || !vax */
				/*
				 * THE FOLLOWING ASSEMBLY CODE IS INSERTED
				 * BECAUSE THE COMPILER CAN'T OPTIMIZE THE
				 * C CODE WORTH A DARN
				 */
				asm("	movzbl	(r10)+,r1 # *rp++ & 255");
				asm("	ashl	r9,r1,r0  # >> rshift");
				asm("	bisb2	r0,(r11)+ # *bp++ |=");
				asm("	ashl	r8,r1,r0  # << lshift");
				asm("	bisb2	r0,(r11)  # *bp |=");
#endif /* lint || !vax */
			}
			if (--row < 0) {
				row = ROWS - 1;
				bp = &VBuffer[ROWS - 1][col];
			} else
				bp -= o;
		}
	}

	j = height + CurRow - 1;/* have now set bits this far */
	if (j >= ROWS)
		j -= ROWS;	/* keep it modular */

	/*
	 * There are two cases.  Either the buffer is not currently wrapped,
	 * in which case the regions past LastRow or before FirstRow extend
	 * it; or it is wrapped, in which case the region between LastRow
	 * and FirstRow extends it:
	 *
	 *	    case 1		 case 2
	 *	   --------		--------
	 *	   |      |	last  ->| XXXX |
	 * first ->| XXXX |		|      |
	 *	   | XXXX |		|      |
	 * last  ->| XXXX |	first ->| XXXX |
	 *	   |      |		| XXXX |
	 *	   --------		--------
	 *
	 * The `X's mark the region that is in use; the blank spaces
	 * mark the region that causes the `last' value to change.
	 */
	if (FirstRow <= LastRow) {
		/* first case: not wrapped */
		if (j < FirstRow || j > LastRow)
			LastRow = j;
	} else {
		/* second case: wrapped */
		if (j > LastRow && j < FirstRow)
			LastRow = j;
	}
}

/*
 * Write a rule at the current row according to the (packed) information in
 * 'info'.  This includes the x position and the height and width of the
 * rule.
 */
void
VWriteRule(info)
	int info;
{
	register unsigned char *bp;	/* buffer pointer */
	register int j;
	register int lbits;	/* bits along left */
	register int rbits;	/* bits along right */
	register int o;		/* offset to next row */
	register int i;
	register int full;	/* number of 8 bit words to set */
	register int height;	/* rule height */
	register int width;	/* rule width */
	register int row;
	register int col;

	i = info;
	CurCol = (i & 0x7fff0000) >> 16;
	height = ((i & 0xff00) >> 8);
	width = (i & 0xff);
	if ( Debug > 1 )
	    fprintf( stderr, "Rule: Row: %d, Col:%d, height:%d, width:%d\n",
		     CurRow, CurCol, height, width );
	col = CurCol >> 3;
	row = CurRow;
	j = CurCol & 7;		/* bit # of start position */
	lbits = 0xff >> j;	/* bits to set along left edge */
	/* there are 8-j bits set in lbits */
	o = 8 - j - width;
	if (o > 0) {		/* then lbits has o too many bits set */
		lbits >>= o;
		lbits <<= o;	/* puts zeros into o righthand bits */
		rbits = 0;
		full = 0;
	} else {
		i = (CurCol + width) & 7;	/* bit # of ending position */
		rbits = 0xff00 >> i;	/* bits to set along right edge */
		/* there are i bits set in rbits (well, in the low byte) */
		full = (width - i - (8 - j)) >> 3;
	}
	bp = &VBuffer[row][col];
	i = height;

	/* Often "full" is zero, which makes things faster */
	if (full) {		/* oh well */
		o = COLUMNS - full - 1;
		while (--i >= 0) {
			*bp++ |= lbits;
			for (j = full; --j >= 0;)
				*bp++ |= 0xff;
			*bp |= rbits;
			if (++row >= ROWS) {
				row = 0;
				bp = &VBuffer[0][col];
			} else
				bp += o;
		}
	} else {
		o = COLUMNS - 1;
		while (--i >= 0) {
			*bp++ |= lbits;
			*bp |= rbits;
			if (++row >= ROWS) {
				row = 0;
				bp = &VBuffer[0][col];
			} else
				bp += o;
		}
	}
	i = CurRow + height - 1;
	if (i >= ROWS)
		i -= ROWS;
	/*
	 * This is another way of expressing both cases 1 and 2 in
	 * VWriteChar().  I think the other way is likely to be
	 * faster, and characters occur far more frequently; but this
	 * is the more readable by far.
	 */
	if (RowsBetween(FirstRow, LastRow) < RowsBetween(FirstRow, i))
		LastRow = i;
}

/*
 * Dump out the top portion of the band (rows [Firstrow, CurRow)).
 */
void
DumpTopOfBand()
{

	/*
	 * To exclude CurRow, subtract one, but modularly, modularly!
	 */
	WriteBuf(&VBuffer[0][0], FirstRow, CurRow ? CurRow - 1 : ROWS - 1, 1);
	FirstRow = CurRow;
}

/*
 * Move the current row in the band buffer down by delta rows, by,
 * if necessary, writing out the currently-used portion of the buffer.
 */
void
MoveDown(delta)
	register int delta;
{

	if (delta >= ROWS - RowsBetween(FirstRow, CurRow)) {
		/*
		 * Need to roll the cylinder forward.  Write out the used
		 * part, and then write as many blank lines as necessary.
		 */
		WriteBuf(&VBuffer[0][0], FirstRow, LastRow, 1);
		WriteBlanks(delta - RowsBetween(CurRow, LastRow) - 1);
		CurRow = LastRow = FirstRow;	/* band is now empty */
	} else {
		/*
		 * Because RowsBetween returns nonnegative integers, we
		 * know delta <= ROWS, so can do mod more quickly thus:
		 */
		CurRow += delta;	/* result < 2*ROWS */
		if (CurRow >= ROWS)
			CurRow -= ROWS;	/* now result < ROWS */
	}
}

/*
 * Count the bits in a byte
 */
static unsigned char nbits[256] = {
      0,  1,  1,  2,  1,  2,  2,  3,  1,  2,  2,  3,  2,  3,  3,  4,
      1,  2,  2,  3,  2,  3,  3,  4,  2,  3,  3,  4,  3,  4,  4,  5,
      1,  2,  2,  3,  2,  3,  3,  4,  2,  3,  3,  4,  3,  4,  4,  5,
      2,  3,  3,  4,  3,  4,  4,  5,  3,  4,  4,  5,  4,  5,  5,  6,
      1,  2,  2,  3,  2,  3,  3,  4,  2,  3,  3,  4,  3,  4,  4,  5,
      2,  3,  3,  4,  3,  4,  4,  5,  3,  4,  4,  5,  4,  5,  5,  6,
      2,  3,  3,  4,  3,  4,  4,  5,  3,  4,  4,  5,  4,  5,  5,  6,
      3,  4,  4,  5,  4,  5,  5,  6,  4,  5,  5,  6,  5,  6,  6,  7,
      1,  2,  2,  3,  2,  3,  3,  4,  2,  3,  3,  4,  3,  4,  4,  5,
      2,  3,  3,  4,  3,  4,  4,  5,  3,  4,  4,  5,  4,  5,  5,  6,
      2,  3,  3,  4,  3,  4,  4,  5,  3,  4,  4,  5,  4,  5,  5,  6,
      3,  4,  4,  5,  4,  5,  5,  6,  4,  5,  5,  6,  5,  6,  6,  7,
      2,  3,  3,  4,  3,  4,  4,  5,  3,  4,  4,  5,  4,  5,  5,  6,
      3,  4,  4,  5,  4,  5,  5,  6,  4,  5,  5,  6,  5,  6,  6,  7,
      3,  4,  4,  5,  4,  5,  5,  6,  4,  5,  5,  6,  5,  6,  6,  7,
      4,  5,  5,  6,  5,  6,  6,  7,  5,  6,  6,  7,  6,  7,  7,  8
};

/*
 * Write the lines between the first and last inclusive from the given
 * buffer.  If 'cl', clear after writing.
 * Filters pixels using a box filter (filth x filtw) in size.
 *
 * If buf is NULL, then just skips blank space.
 */
void
WriteBuf(buf, first, last, cl)
	unsigned char *buf;
	register int first, last;
	int cl;
{
    	static unsigned char bytebuf[COLUMNS*8];
	static int filtrow = 0, sawbit = 0;
	unsigned char mask;
	unsigned char * scans[2];
	int row;
	register int bit, col;
	register unsigned char * cp, * bufp;

	if ( Debug > 1 )
	    fprintf( stderr, "Writebuf( %lx, %d, %d, %d )\n",
		     buf, first, last, cl );

	/* Setup RLE file if beginning of page */
	if ( NewPage )
	{
	    rle_put_setup( &out_hdr );
	    NewPage = 0;
	}

	scans[0] = scans[1] = bytebuf;

	if ( buf == NULL )
	{
	    if ( filtrow > 0 && filtrow + last - first + 1 >= filth )
	    {
		for ( col = 0; col < outwidth; col++ )
		{
		    /* Why two lines?  HP compiler bug! */
		    bit = bytebuf[col] * filtfact;
		    bytebuf[col] = bit;
		}
		rle_putrow( &scans[1], outwidth, &out_hdr );
		for ( col = 0; col < outwidth; col++ )
		    bytebuf[col] = 0;
	    }
	    NLines += last - first + 1;
	    if ( last - first - filtrow + 1 >= filth )
		rle_skiprow( &out_hdr,
			    (last - first - filtrow + 1) / filth );
	    filtrow = (filtrow + last - first + 1) % filth;
	    return;
	}

	if (first > last) {	/* recursively do wrapped part first */
		WriteBuf(buf, first, ROWS - 1, cl);
		first = 0;
	}
	buf = &buf[first * COLUMNS];

	mask = (0xff << (8 - filtw)) & 0xff;
	for ( row = first, bufp = buf; row <= last; row++, bufp += COLUMNS )
	{
	    for ( cp = bufp, col = 0, bit = 0; col < outwidth;
		  col++, bit += filtw, bit >= 8 ? cp++ : cp, bit %= 8 )
		if ( (bytebuf[col] +=
		      nbits[(*cp & (mask >> bit))] +
		      nbits[((*(cp+1) & (mask << (8 - bit)))) & 0xff]) != 0 )
		    sawbit++;

	    filtrow = (filtrow + 1) % filth;

	    if ( filtrow == 0 )
	    {
		if ( sawbit )
		{
		    for ( col = 0; col < outwidth; col++ )
		    {
			bit = bytebuf[col] * filtfact;
			bytebuf[col] = bit;
		    }
		    rle_putrow( &scans[1], outwidth, &out_hdr );
		    for ( col = 0; col < outwidth; col++ )
			bytebuf[col] = 0;
		}
		else
		    rle_skiprow( &out_hdr, 1 );
		sawbit = 0;
	    }
	}

	if (cl)
		bzero(buf, (unsigned) (COLUMNS * (last - first + 1)));
	NLines += last - first + 1;
}

/*
 * Write 'n' blank lines.
 */
void
WriteBlanks(n)
	int n;
{
	WriteBuf((unsigned char *)0, 0, n - 1, 0);
}

/*
 * Perform a page feed.
 */
void
FormFeed()
{
    if ( !NewPage )
	rle_puteof( &out_hdr );
    NewPage = 1;		/* next output will setup RLE file */
}

#ifdef hpux
void
bcopy( from, to, len )
{
    return memcpy( to, from, len );
}

bzero( to, len )
{
    memset( to, 0, len );
}

#endif
