/* 
 * qcrldmap.c - Load a color map on the QCR-Z
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Jan 21 1988
 * Copyright (c) 1988, University of Utah
 */

#include <stdio.h>

main( argc, argv )
int argc;
char ** argv;
{
    int file_flag = 0, mapflag = 0, map_num, verbose_flag = 0, i;
    char * filename;
    FILE * mapfile;
    unsigned char lut_data[1536];
    unsigned char * lutptr;
    int num;
    
    if (! scanargs( argc, argv, "% f%-file!s m%-stdmap!d v%-", &file_flag,
		   &filename, &mapflag, &map_num, &verbose_flag ))
    {
	fprintf(stderr, "std color maps are: 1=linear, 2=Polaroid 59,\n");
	fprintf(stderr, "  3=Ektachrome 100, 4=Ektachrome 100 (4K),\n");
	fprintf(stderr, "  5=Polaroid 59 (4K), 6=Polaroid 809 (4K)\n");
	exit(-2);
    }

    if (mapflag && file_flag)
    {
	fprintf(stderr, "qcrldmap: only one of -f or -m\n");
	exit(-2);
    }

    init_qcr( verbose_flag );

    if (mapflag)
    {
	qcr_load_i_luts( map_num );
    }

    if (file_flag)
    {
	if (! (mapfile = fopen( filename, "r" )))
	{
	    perror("qcrldmap");
	    exit(-2);
	}

	lutptr = lut_data;
	for (i = 0; i < 3*256; i++)
	{
	    fscanf( mapfile, "%d", &num );
	    *lutptr++ = (num >> 8) & 0xFF;
	    *lutptr++ = num & 0xFF;
	}
	qcr_ld_lut12( lut_data );
    }
}
