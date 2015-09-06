/* 
 * XtndRunget.c - Extended Run Length Encoded retrieval.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Aug 10 1982
 * Copyright (c) 1982 Spencer W. Thomas
 * $Id: XtndRunget.c,v 3.0.1.1 90/11/19 16:45:12 spencer Exp $
 * $Log:	XtndRunget.c,v $
 * Revision 3.0.1.1  90/11/19  16:45:12  spencer
 * patch2: Add rle_config.h.
 * 
 * Revision 3.0  90/08/03  15:26:10  spencer
 * Establish version 3.0 base.
 * 
 * Revision 1.3  90/07/13  15:21:42  spencer
 * Add void*malloc().  Cast malloc calls.
 * 
 * Revision 1.2  90/06/18  23:53:24  spencer
 * Get rid of sv_ names.
 * 
 * Revision 1.1  90/06/18  23:50:31  spencer
 * Initial revision
 * 
 * Revision 2.10  86/10/08  13:10:55  thomas
 * typo fix.
 * 
 * Revision 2.9  86/10/08  13:01:57  thomas
 * Add ability to read (and print) picture comments.
 * Recognize EOF opcode.
 * 
 * Revision 2.8  86/03/10  15:45:29  thomas
 * Fix up getchan/putchan.
 * 
 * Revision 2.7  86/02/27  10:03:39  thomas
 * fprintf on stderr, not on a string.
 * 
 * Revision 2.6  86/02/25  17:31:13  thomas
 * Add channel munging (get single channel capability).
 * 
 * Revision 2.5  85/04/26  15:04:46  thomas
 * Lint fixes.
 * New flags and variable length background color.
 * 
 * Revision 2.4  85/04/06  01:21:42  thomas
 * Was only right justifying first channel of color map.
 * 
 * Revision 2.3  85/04/02  00:41:26  thomas
 * Move "comment" code in Byte and Run code.
 * 
 * Revision 2.2  85/03/05  21:17:28  thomas
 * Make sure to not put more than 510 bytes in a byte unpacker operation.
 * 
 * Revision 2.1  85/03/05  15:59:17  thomas
 * *** empty log message ***
 * 
 * Revision 1.3  84/03/12  21:29:15  thomas
 * Add "bread" routine, reduce output buffer size.
 * 
 */

#include "rle_config.h"
#include "stdio.h"
#include "getfb.h"
#include "rle_code.h"
#include <fb.h>
#ifndef VOID_STAR
extern char * malloc();
#else
extern void *malloc();
#endif

#define BREAD(type, var, len)\
	if ( fd->_cnt < len )\
	    bread( (char *)&var, len, fd );\
	else\
	{\
	    var = *(type *)fd->_ptr;\
	    fd->_ptr += len;\
	    fd->_cnt -= len;\
	}

#define OPCODE(inst) (inst.opcode & ~LONG)
#define LONGP(inst) (inst.opcode & LONG)
#define DATUM(inst) (0x00ff & inst.datum)

#ifdef BIGENDIAN
#define SWAB(shrt)  (shrt = ((shrt >> 8) & 0xff) | ((shrt << 8) & 0xff00))
#else
#define	SWAB(shrt)
#endif

extern int cflag, getchan, putchan;	/* to load single channel */


/* ARGSUSED */
XtndRunGet(magic)
short magic;
{
    struct XtndRsetup   setup;
    unsigned char * bg_color;
    register short * fbptr;
    register struct fd_file * fbbuf =
		(struct fd_file *) ((char *) buffer + 1024);
    register int fbx, fby, n;
    int i, j, len;
    int fbbufsize = (sizeof buffer - 1024) / sizeof(short);
    short word, long_data;
    long maplen;
    struct inst {
	short opcode:8,
	    datum:8;
    } inst;
    char * comment_buf, ** comments;

    if ( fbbufsize > 2048 ) fbbufsize = 2048;	/* make it look faster */
    if (bread ((char *)&setup, SETUPSIZE, fd) < 1)
    {
	fprintf (stderr, "getgmr: Couldn't read setup information\n");
	exit (1);
    }
    /* 
     * Machines with big-endian byte order need to swap some bytes on
     * input.
     */
    SWAB( setup.h_xpos );
    SWAB( setup.h_ypos );
    SWAB( setup.h_xlen );
    SWAB( setup.h_ylen );

    /* 
     * Get background colors if present.
     */
    if ( !(setup.h_flags & H_NO_BACKGROUND) )
    {
	bg_color = (unsigned char *)malloc( 1 + (setup.h_ncolors/2) * 2 );
	bread( (char *)bg_color, 1 + (setup.h_ncolors/2) * 2, fd );
    }
    else
	bread( (char *)&word, 1, fd );	/* skip filler byte */

    /*
     * Read color map if present
     */
    if (setup.h_ncmap)
    {
	maplen = 1 << setup.h_cmaplen;
	if ( maplen * setup.h_ncmap > (sizeof buffer) )
	{
	    fprintf( stderr,
		     "Color map too big (%d entries total), giving up\n",
		     maplen * setup.h_ncmap );
	}
	for ( i = 0; i < setup.h_ncmap; i++ )
	{
	    bread((char *)&buffer[maplen*i], sizeof(short) * maplen, fd);
	    for ( j = 0; j < 256; j++ )
		if ( j >= maplen )
		    buffer[j + 256*i] = 0;	/* fill in short map */
		else
		    buffer[j + 256*i] = ( buffer[j + 256*i] >> 8 ) & 0xff;
	}
	/* 
	 * If < 3 channels in color map, copy from the first into the
	 * missing ones.
	 */
	for ( ; i < 3; i++ )
	    for ( j = 0; j < 255; j++ )
		buffer[j + 256 * i] = buffer[j];
    }

    /*
     * Read comments if present
     */
    if ( setup.h_flags & H_COMMENT )
    {
	short comlen, evenlen;
	register char * cp;

	/* get comment length */
	BREAD( short, comlen, sizeof comlen );	/* get comment length */
	SWAB( comlen );
	evenlen = (comlen + 1) & ~1;	/* make it even */
	comment_buf = (char *)malloc( (unsigned) evenlen );
	if ( comment_buf == NULL )
	{
	    fprintf( stderr,
		     "getgmr: Malloc failed for comment buffer of size %d\n",
		     comlen );
	    return -2;
	}
	fread( comment_buf, 1, evenlen, fd );
	/* Count the comments */
	for ( i = 0, cp = comment_buf; cp < comment_buf + comlen; cp++ )
	    if ( *cp == 0 )
		i++;
	i++;			/* extra for NULL pointer at end */
	/* Get space to put pointers to comments */
	comments = (char **)malloc( (unsigned)(i * sizeof(char *)) );
	if ( comments == NULL )
	{
	    fprintf( stderr,
		    "getgmr: Malloc failed for %d comment pointers\n",
		     i );
	    return -2;
	}
	/* Get pointers to the comments */
	*comments = comment_buf;
	for ( i = 1, cp = comment_buf + 1; cp < comment_buf + comlen; cp++ )
	    if ( *(cp - 1) == 0 )
		comments[i++] = cp;
    }


    if (query)
    {
	if ( setup.h_ncolors != 3 )
	    fprintf( stderr, "%d color channels were saved.\n",
		    setup.h_ncolors);
	if ( setup.h_flags & H_ALPHA )
	    fprintf( stderr, "Alpha channel was saved.\n" );
	fprintf (stderr, "Originally positioned at (%d, %d), size (%d %d)\n",
		setup.h_xpos, setup.h_ypos, setup.h_xlen, setup.h_ylen);
	if ( setup.h_flags & H_CLEARFIRST )
	    fprintf( stderr, "Screen will be cleared to " );
	else if ( !(setup.h_flags & H_NO_BACKGROUND) )
	    fprintf( stderr, "Saved in overlay mode with original " );
	if ( setup.h_flags & H_NO_BACKGROUND )
	    fprintf( stderr, "No background color was saved.\n" );
	else
	{
	    fprintf (stderr, "background color" );
	    for ( i = 0; i < setup.h_ncolors; i++ )
		fprintf( stderr, " %d", bg_color[i] );
	    fprintf( stderr, "\n" );
	}

	if (setup.h_ncmap)
	{
	    fprintf (stderr,
		     "%d channels of color map %d entries long were saved.\n",
		     setup.h_ncmap, 1 << setup.h_cmaplen );
	}

	if ( setup.h_flags & H_COMMENT )
	{
	    fprintf( stderr, "Comments:\n" );
	    for ( i = 0; comments[i] != NULL; i++ )
		fprintf( stderr, "%s\n", comments[i] );
	}
	exit (0);
    }

    Fbinit();

    /* 
     * Handle -c getchan [putchan] option.
     * 
     * If both getchan & putchan are specified, take the input data in
     * the channel (getchan) and place it on the fb in the channel
     * (putchan).  If only getchan is specified, put the data from the
     * input channel into the same color channel in the fb.
     * 
     * As a special case, -c getchan acts as -c 0 getchan for a B&W
     * image (this accounts for much of the extra complexity of the
     * following block).
     */

    if ( cflag )
    {
	if ( getchan > setup.h_ncolors && setup.h_ncolors > 1 )
	{
	    fprintf( stderr,
		    "Requested channel (%d) not in input file (%d channels)\n",
		    getchan, setup.h_ncolors );
	    exit( 1 );
	}
	if ( getchan < 0 && (setup.h_flags & H_ALPHA) == 0 )
	{
	    fprintf( stderr, "getgmr: No alpha channel in input file\n" );
	    exit( 1 );
	}
	if ( putchan == -1 )
	    putchan = getchan;
	if ( putchan < 0 || putchan > 2 )
	{
	    fprintf( stderr,
		"getgmr: Requested output channel (%d) doesn't exist.\n",
		putchan );
	    exit( 1 );
	}
	if ( setup.h_ncolors == 1 && getchan > 0 )
	    getchan = 0;
    }

    if (posflag)
    {
	fprintf (stderr, "Originally positioned at (%d, %d)\n",
		setup.h_xpos, setup.h_ypos);
	if (posflag == 1)		/* incremental positioning */
	{
	    setup.h_xpos = (x + setup.h_xpos) & 0777;
	    setup.h_ypos = (y + setup.h_ypos) & 0777;
	}
	else
	{
	    setup.h_xpos = x & 0777;
	    setup.h_ypos = y & 0777;
	}
    }
    if (background == 0)
    {
	if (setup.h_flags & H_CLEARFIRST)
	    background = 2;
	else
	    background = 1;
    }

    if ( setup.h_ncmap )
	Fmbput(0, buffer);		/* write color map */

    Fdstart(fbbuf, fbbufsize, 0);
					/* do some output buffering */
    if (background == 2)
    {
	if ( cflag )
	{
	    Fchn( 1 << putchan );
	    Frvput( 0, 511, 0, 511, bg_color[getchan] );
	}
	else
	    if ( setup.h_ncolors >= 3 )
		Fr3put(0, 511, 0, 511, bg_color[0], bg_color[1], bg_color[2]);
	    else
		Fr3put(0, 511, 0, 511, bg_color[0], bg_color[0], bg_color[0]);
    }

    fbx = setup.h_xpos;
    fby = setup.h_ypos;

    /* This is a middle exit loop */
    for (;;)
    {
        BREAD(struct inst, inst, 2 );
	if ( feof(fd) ) break;

	switch( OPCODE(inst) )
	{
	case RSkipLinesOp:
	    if ( LONGP(inst) )
	    {
	        BREAD( short, long_data, sizeof long_data );
		SWAB( long_data );
		fby += long_data;
		if (debug)
		    fprintf(stderr, "Skip Lines %d\n", long_data);
	    }
	    else
	    {
		fby += DATUM(inst);
		if (debug)
		    fprintf(stderr, "Skip Lines %d\n", inst.datum);
	    }
	    break;

	case RSetColorOp:
	    if ( cflag )
	    {
		if ( (getchan&0xff) == DATUM(inst) )
		    Fchn( 1 << putchan );
		else
		    Fchn( 0 );
	    }
	    else
	    {
		if (setup.h_ncolors == 1)
		    Fchn(7);
		else
		    Fchn(1<<DATUM(inst));	/* select color channel */
	    }
	    fbx = setup.h_xpos;
	    if (debug)
		fprintf(stderr, "Set Color %d\n", inst.datum);
	    break;

	case RSkipPixelsOp:
	    if ( LONGP(inst) )
	    {
	        BREAD( short, long_data, sizeof long_data );
		SWAB( long_data );
		fbx += long_data;
		if (debug)
		    fprintf(stderr, "Skip Pixels %d\n", long_data);
	    }
	    else
	    {
		fbx += DATUM(inst);
		if (debug)
		    fprintf(stderr, "Skip Pixels %d\n", DATUM(inst));
	    }
	    break;

	case RByteDataOp:
	    if ( LONGP(inst) )
	    {
	        BREAD( short, long_data, sizeof long_data );
		SWAB( long_data );
		n = (int)long_data;
	    }
	    else
	    {
		n = DATUM(inst);
	    }
/*	    Fbvput( fbx, fbx+n, fby, fby, buffer ); */
	    n++;
	    if (debug)
		fprintf(stderr, "Byte Data, length %d\n", n);
	    do {
		len = n <= 510 ? n : 510;	/* blasted byteunpacker */
		_Fdreserve( (len+1)/2 + 7 );
		fbptr = &fbbuf->fd_list[fbbuf->fd_len];
		*fbptr++ = SelectPeripheralDevice | ByteUnpacker;
		*fbptr++ = LoadEA | (fbx & 0777);
		*fbptr++ = LoadEB | 1;
		*fbptr++ = LoadLA | (fby & 0777);
		*fbptr++ = LoadUpdateMode | A_AB;
		*fbptr++ = LoadPRegister | (1<<9) | ((len & 1) << 8) |
						    ((len + 1) >> 1);
		bread((char *)fbptr, (len+1) & ~1, fd); /* Round up to multiple of 2 */
		fbptr += (len+1)/2;
		fbx += len;
		fbbuf->fd_len = fbptr - fbbuf->fd_list;
		n -= len;
	    } while ( n > 0 );
	    break;

	case RRunDataOp:
	    if ( LONGP(inst) )
	    {
	        BREAD( short, long_data, sizeof long_data );
		SWAB( long_data );
		n = long_data;
	    }
	    else
	    {
		n = DATUM(inst);
	    }
	    BREAD( short, word, sizeof(short) );
/*	    Frvput(fbx, fbx+n, fby, fby, word);*/
	    _Fdreserve( 5 );
	    fbptr = &fbbuf->fd_list[fbbuf->fd_len];
	    *fbptr++ = LoadEA | (fbx & 0777);
	    *fbptr++ = LoadEB | n;
	    *fbptr++ = LoadLA | (fby & 0777);
	    *fbptr++ = LoadSubchannelMask | (word & 07777);
	    *fbptr++ = LoadLB | GoWrite;
	    fbbuf->fd_len = fbptr - fbbuf->fd_list;
	    fbx += n+1;
	    if (debug)
		fprintf(stderr, "Run, length %d, color %d\n", n+1, word);
	    break;

	case REOFOp:
	    break;

	default:
	    fprintf(stderr, "getgmr: Unrecognized opcode: %d\n", OPCODE(inst));
	    Fdstop();
	    Fddraw(fbbuf);		/* flush the buffer */
	    exit(1);
	}
	if ( OPCODE( inst ) == REOFOp )
	    break;		/* end of input here */
    }
    Fdstop();
    Fddraw(fbbuf);			/* flush the buffer */
}
