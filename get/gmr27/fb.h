/* 
 * fbreg.h - Register and other definitions for Grinnell FB.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Dec 20 1983
 * Copyright (c) 1983 Spencer W. Thomas
 * 
 * $Header: fbreg.h,v 1.1 84/01/05 15:58:56 thomas Exp $
 * $Log:	fbreg.h,v $
 * Revision 1.1  84/01/05  15:58:56  thomas
 * Initial revision
 * 
 */


#ifndef GRINNELL_H                            /* only once */
#define GRINNELL_H

#ifdef KERNEL
struct fbdevice
{
    short   drwc;			/* DR11 word count */
    u_short drba;			/* DR11 buffer address */
    short   drst;			/* DR11 status register */
    short   drdb;			/* not used */
};


/* various defines */

#define FBPRI		(NZERO + 1)	/* sleep priority */
#define FBTOUT		600		/* 10 sec timeout */

/* DR11 control bits */

#define	GO	             01
#define	FNCT1		     02
#define	INT_ENAB	   0100
#define	READY		   0200
#define	ATTN		 020000
#define	NEX		 040000
#define	ERROR		0100000

/* Frame buffer status register interpretation string */

#define	FBSTAT	"\020\020ERROR\017NEX\016ATTN\010READY\07IE\02F1\01GO"

#define	FBADDR(unit)	((struct fbdevice *)fbinfo[unit]->ui_addr)

/* transmission modes */

#define	READ		INT_ENAB | FNCT1 | GO	/* cpu <- fb */
#define WRITE		INT_ENAB | GO	/* cpu -> fb */

#endif KERNEL

/*
	definitions of basic frame buffer instructions
*/

#define	WriteImageData		(short)0000000
#define	LoadSubchannelMask	(short)0010000
#define	WriteGraphicData	(short)0020000
#define	Text			(short)0022000
#define	LoadWriteMode		(short)0024000
#define	Background		(short)0200	/* Reverse Background Bit */
#define	Additive		(short)0100	/* Additive Characters/Graphic Data */
#define	ZBit			(short)040	/* the "Z-Bit" */
#define	VectorMode		(short)020	/* Vector Mode Bit */
#define	ModeHeight		(short)010
#define	ModeWidth		(short)04
#define	CursPos			(short)02	/* if on, position at A+B, else at A */
#define	CursOn			(short)01	/* Cursor visible or not */
#define	LoadUpdateMode		(short)0026000
#define	ElementUpdate		(short)0/* shift for E1E0 bits */
#define	LineUpdate		2	/* shift for L1L0 bits */
#define	ScrUpdate		4	/* shift for S1S0 bits */
#define	A_no_change		(short)0/* Ea or La remains unchanged */
#define	A_C			1	/* Ea or La gets Ec or Lc */
#define	A_AB			2	/* Ea (La) gets Ea+Eb (La+Lb) */
#define	A_AC			3	/* Ea (La) gets Ea+Ec (La+Lc) */
#define	NoScroll		(short)0/* no scrolling */
#define	ScrollHome		1	/* scroll to home position */
#define	ScrollDown		2
#define	ScrollUp		3
#define	Erase			(short)0030000
#define	EraseLine		(short)0032000
#define	SpecialLocationUpdate	(short)0034000
#define	ExecuteGraphicWrite	(short)0036000
#define	GoWrite			(short)0002000
#define	LoadElement		(short)0040000
#define	LoadLine		(short)0060000
#define	LoadRelative		(short)0000000
#define	LoadARegisters		(short)0004000
#define	LoadBRegisters		(short)0010000
#define	LoadCregisters		(short)0014000
#define	LoadEaRelative		(short)0040000
#define	LoadEA			(short)0044000
#define	LoadEB			(short)0050000
#define	LoadEC			(short)0054000
#define	LoadLaRelative		(short)0060000
#define	LoadLA			(short)0064000
#define	LoadLB			(short)0070000
#define	LoadLC			(short)0074000
#define	LoadDisplayChannels	(short)0100000
#define	SelectPeripheralDevice	(short)0120000
#define	Digitizer		(short)01
#define	VideoControl		(short)020	/* Video Configuration */
#define	ColorMap		(short)040	/* Bit 5 */
#define SelectRedMap		(short)0
#define SelectGreenMap		(short)02000
#define	SelectBlueMap		(short)04000
#define SelectWriteAll		(short)06000
#define Zoom			(short)0200	/* Zoom and Pan card (Bit 7) */
#define ZoomOn			(short)010	/* Enable Zoom and Pan */
#define ZcursOn			(short)040	/* Zoom cursor visible or not */
#define ZcBlink			(short)020	/* Blink Zoom cursor or not */
#define Zwrap			(short)04	/* Wrap or Clip */
#define Zfactor1		(short)0/* times one */
#define Zfactor2		(short)1/* times two */
#define Zfactor4		(short)2/* times four */
#define Zfactor8		(short)3/* times eight */
#define	MemReadback		(short)0400	/* Memory Readback */
#define	ByteUnpacker		(short)01000
#define UnpackText		(short)07000	/* Text and bit 9. */
#define UnpackGraphicData	(short)05000	/* Graphic Data and bit 9. */
#define UnpackImageData		(short)03000	/* Image Data and bit 9. */
#define	IntTest			(short)04000	/* Internal Tests */
#define	LoadPAddress		(short)0130000
#define	LoadPRegister		(short)0140000
#define	PRegShift		9	/* shift to Peripheral
					   Register Field */
#define	LoadPData		(short)0150000
#define	ReadbackPData		(short)0160000
#define	NoOperation		(short)0170000
#define	BUG			1
					/* bug exists which eats 2nd
					   word of some transfers */
#define ZBUGy			4
					/* bug exists where Zoom Cursor
					   is 4 pixels above mark. */
#ifndef KERNEL

/* TAG( fd_file )
 * 
 * Display file structure as used by Fdstart, Fddraw, etc.
 */

struct	fd_file	{	/* definition of display file structure */
	int	fd_len;		/* length used */
	int	fd_abs_rel;	/* absolute/relative flag */
	int	fd_x;		/* final x position */
	int	fd_y;		/* final y position */
	short	fd_list[1];	/* actual display file */
};

#endif KERNEL

/* Definition of IOCTL codes */
#define	FBGETBOX	_IOW(F,0,struct fb_getbox)

/* Structure used by FBGETBOX ioctl */
struct fb_getbox {
    short xmin, xmax,
	  ymin, ymax,
	  xfreq, yfreq;
    short *buf;
    int n;
};

#endif GRINNELL_H
