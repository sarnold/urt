/* 
 * getfb.c - Display Extended Run Length Encoding with Alpha1 channel
 *		on BRL libfb frame buffer.
 * 
 * Author:	Paul R. Stay
 * 		Ballistic Research Labratory
 * 		Aberdeen Proving Ground, Md.
 * Date:	Mon Jun 23 1986
 */

#include <stdio.h>
#include <brlcad/fb.h>
#include "rle.h"

#define MAX_PIXELS   1024

main( argc, argv)
int argc;
char ** argv;
{
    	rle_hdr hdr;
	FILE *rle_fd;
	FBIO *fbp;
	int r_debug;
	char * rle_file;
	register int i,j;

	unsigned char scanline[4][MAX_PIXELS];	/* Run length scan line */
	unsigned char *rows[4];		/* Character pointers for rle_getrow */
	
	RGBpixel pix[MAX_PIXELS];		/* pixel structure */
	int x_len, y_len;

	hdr = *rle_hdr_init( (rle_hdr *)NULL );
	rle_names( &hdr, cmd_name( argv ), NULL, 0 );
	argc--; argv++;
	while( argv[0][0] == '-')  {

		switch( argv[0][1] )  {
		case 'D':
			r_debug = 1;
			break;
		default:
			break;
		}
		argc--; argv++;
	}

	rle_file = argv[0];
	rle_names( &hdr, hdr.cmd, rle_file );
        hdr.rle_file = rle_open_f(hdr.cmd, rle_file, "r");
	if( rle_get_setup( &hdr ) < 0 )  {
		fprintf(stderr, "getfb: Error reading setup information\n");
		exit(1);
	}

	if (r_debug)
	{
		fprintf( stderr,"Image bounds\n\tmin %d %d\n\tmax %d %d\n",
			hdr.xmin, hdr.ymin,
			hdr.xmax, hdr.ymax );
		if ( hdr.alpha )
			fprintf( stderr, "Alpha Channel present\n");
		rle_debug(1);
	}

	/* We`re only interested in R, G, & B */
	RLE_CLR_BIT(hdr, RLE_ALPHA);
	for (i = 3; i < hdr.ncolors; i++)
		RLE_CLR_BIT(hdr, i);

	x_len = hdr.xmax - hdr.xmin + 1;
	y_len = hdr.ymax - hdr.ymin + 1;

	if( (fbp = fb_open( "", hdr.xmax+1, hdr.ymax+1 )) == NULL )
		exit(12);

	for ( i = hdr.ymin; i <= hdr.ymax; i++)  {
		rows[0] = (unsigned char *)-1;
		rows[1] = scanline[1];	
		rows[2] = scanline[2];	
		rows[3] = scanline[3];	
		rle_getrow(&hdr, &rows[1] );

		for ( j = 0; j < x_len; j++)
		{
			pix[j][RED] = scanline[1][j];
			pix[j][GRN] = scanline[2][j];
			pix[j][BLU] = scanline[3][j];
		}
		fb_write( fbp, hdr.xmin, i, pix, x_len );
	}
	exit(0);
}
