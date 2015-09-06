/* 
 * getfb.h - Global declarations for getfb.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Aug 10 1982
 * Copyright (c) 1982 Spencer W. Thomas
 */

/* 
 * There is a very strange bug in the GrinnellGet routine.  When using a
 * 60*512 short buffer the frame buffer restore screws up.  When using
 * the buffer here, it works ok.  Arghhhh!
 */
short buffer[59*512];			/* biggest allowed by minphys! */
extern int x, y, posflag, background, query, debug;
FILE *fd;
