/*
 * Print all sorts of QCR status.   "-l" lists the color tables.
 */

#include <stdio.h>
char *lables[4] = { "Red", "Green", "Blue", "Neutral" };
main(argc, argv)
int argc;
char **argv;
{
    unsigned char buf[BUFSIZ];
    short lut[3][256];
    int i;
    int print_luts = (strcmp(argv[1],"-l") == 0);

    init_qcr( 1 );

    printf("\n");
    print_qcr_status();
    printf("\n");

/*     qcr_load_i_luts( 2 ); */

    qcr_rd_brt_tbl( buf );
    for ( i = 1; i <= buf[0]; i++ )
	printf( "Bright %d: %3d\n", i - 1, buf[i] );
	
    qcr_rd_brt_lvl( buf );
    for ( i = 0; i < 4; i++ )
	printf( "%s bright: %d\n", lables[i], buf[i] );
	
    if (print_luts)
    {
	qcr_rd_lut12( lut );
	for ( i = 0; i < 256; i++ )
	{
	    printf( "%3d:\t%3d %3d %3d", i, lut[0][i], lut[1][i], lut[2][i] );
	    if ( i != 0 )
		printf( "\t%4d %4d %4d", lut[0][i] / i,
		        lut[1][i] / i, lut[2][i] / i );
	    printf( "\n" );
	}
    }
}


