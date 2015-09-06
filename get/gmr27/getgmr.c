/* 
 * getgmr.c - Copy a file to the frame buffer.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	1 April 1981
 * Copyright (c) 1981 Spencer W. Thomas
 * 
 * $Id: getgmr.c,v 3.0.1.2 1992/01/28 18:51:16 spencer Exp $
 */

#include "rle_config.h"
#include "stdio.h"
#include "getfb.h"
#include "rle_code.h"

int x = 0, y = 0, posflag = 0, background = 0, query = 0, debug = 0,
    cflag = 0, getchan, putchan = -1;

main(argc, argv)
char **argv;
{
	char * fname = NULL;
	short magic;

	if (scanargs(argc, argv,
		    "% D%- q%- BO%- Pi%-x!dy!d c%-channel!dinto%d file%s ",
			&debug,
			&query, &background, &posflag, &x, &y,
			&cflag, &getchan, &putchan, &fname) == 0)
	    exit(1);

        fd = rle_open_f("getgmr", fname, "r");
	if ((!query) && Fbopen() < 0)
	    exit(1);

	if (fread((char *)&magic, sizeof(short), 1, fd) != 1)
	{
	    fprintf(stderr, "Can't read magic number\n");
	    exit(1);
	}

	switch(magic)
	{
	case (RLE_MAGIC):
	    if (query)
		fprintf(stderr, "Image saved in Run Length Encoded form\n");
	    XtndRunGet(magic);
	    break;

	default:
	    fprintf(stderr,
		"File not an RLE file, can't restore (magic=0x%x)\n",
		magic);
	    exit(1);
	    break;
	}
    return 0;
}
