/***************************************************************************
 *
 * qcr.c - Interface commands for a Matrix QCR-Z running on an HP 200/300
 *         machine running hp-ux version 5.1 or greater.  It uses the device
 *         files /dev/qcrc for the command address, /dev/qcrd for the data
 *         address, and /dev/rhpib for the raw HPIB control.
 *
 *
 *      Mark J. Bradakis, University of Utah. Copyright 1987.
 *      Several modifications by John W. Peterson, U. of Utah, 1988.
 *
 ***************************************************************************/

#include "qcr.h"

int qcr_cmd, qcr_data, qcr_raw;
short verbose_flag;

unsigned char response[BUFSIZ];

/*************************************************************************
    Wait for the HPIB SRQ line to be asserted (this is used by the QCR to
    indicate completion of some operation).
*/

qcr_wait_srq()
{
    int i, status;

    io_timeout_ctl( qcr_raw, 90000000 ); /* 1.5 minutes */

    if (hpib_status_wait( qcr_raw, 1 ) == 0)
    {
	/* Clear the assertion.  Assumes the default HPIB address. */
	if ((status = hpib_spoll( qcr_raw, 2 )) == -1)
	{
	    fprintf(stderr, "qcr.c: error during HPIB s-poll\n");
	}
	if ((status = hpib_spoll( qcr_raw, 3 )) == -1)
	{
	    fprintf(stderr, "qcr.c: error during HPIB s-poll\n");
	}
	return(1);
    }
    else
	fprintf(stderr, "qcr.c: HPIB timeout or error (no SRQ)\n");
}

/*************************************************************************
    write_qcr takes one of the command values (listed in qcr.h) and sends
    it to the command address.  If the command is not successfully written,
    it jumps to the reset routine, hoping to salvage something.
 */

write_qcr_cmd(cmd)
int cmd;
{
    char ch[1];

    ch[0] = cmd & 0xff;
    write(qcr_cmd,ch,1);
    /*  if (hpib_bus_status(qcr_raw,1) == 1)  return(reset_qcr()); */
    return(1);			/* Assume success */
}



/*************************************************************************
    write_data: Set up the interface for large transfers using burst mode
    HP-IB communications.

 */

write_data(buff,len)
char *buff;
int len;
{
    int stat;

    if (io_burst(qcr_data,1) != 0) /* Enable burst mode */
	fprintf(stderr," write_data: burst mode on failed %d\n");

    stat = write(qcr_data,buff,len);
    if (stat != len) fprintf(stderr," write_data: stat %d != %d (len)\n",
			     stat, len);

    /*  if (hpib_bus_status(qcr_raw,1) == 1)  return(reset_qcr()); */

    if (io_burst(qcr_data,0) != 0) /* Disable burst mode */
	fprintf(stderr," write_data: burst mode off failed %d\n");

    return(stat);		/* Assume success */
}



/*************************************************************************
    This routine opens the device files required for the QCR command port,
    the data port and the raw hpib control device.  It checks for the
    existence of the device by an identify command.
 */

int
init_qcr(verbose)

short verbose;
{
    int len;
    char ch, *command;

    verbose_flag = verbose;	/* Some control of output */

    if ((qcr_cmd = open ("/dev/qcrc", O_RDWR)) < 0)
    {
	fprintf(stderr,"Recorder command dev not opened: error = %d\n",errno);
	return(-1);
    }
    if ((qcr_data = open ("/dev/qcrd", O_RDWR)) < 0)
    {
	fprintf(stderr,"Recorder data dev not opened: error = %d\n",errno);
	return(-1);
    }

    if ((qcr_raw = open ("/dev/rhpib", O_RDWR)) < 0)
    {
	fprintf(stderr,"/dev/rhpib not opened: error = %d\n",errno);
	return(-1);
    }
    /* Check for response from the Matrix */

    write_qcr_cmd(GET_VERSION_ID);

    if (read(qcr_data,response,BUFSIZ) != IDLENGTH )
    { 
	fprintf(stderr,"Error reading response from QCR, errno = %d\n",errno);
	return(-1);
    }

    if (verbose_flag)
    {
	fprintf(stderr,"\n\n Matrix Camera Control, Version 0.1\n\n");
	fprintf(stderr,"   matrix rev %s\n",response);
	write_qcr_cmd(MODULE_ID);
	len = read(qcr_data,response,BUFSIZ);
	switch (response[0])
	{
	case 0x10: command = "M35"; break;
	case 0x20: command = "M120"; break;
	case 0x40: command = "M240"; break;
	case 0xf0: command = "Empty"; break;
	default: sprintf(command,"Unknown (%x) ",response[0]); break;
	}
	fprintf(stderr, "Camera back: %s\n", command);
    }
}

/*************************************************************************

  Print the current status of the QCR

*/
#define IS_ONE(resp_num, msg1, msg2) if (((int)response[resp_num + 1]) == 1) \
				    printf(msg1); else printf(msg2);
print_qcr_status()
{
    int len, i, j;
    unsigned char * rp;

    write_qcr_cmd( RETURN_STATUS );
    len = read( qcr_data, response, BUFSIZ );
    printf("Got back %d status bytes\n", len);

    if (verbose_flag)
    {
	printf("Status data (in hex):\n");
	rp = response;
	for (j = 0; j < 2; j++)
	{
	    for (i = 0; i < 16; i++)
		printf("%2X ", (int) *rp++);
	    printf("\n");
	}
    }

    rp = response;
    rp++;			/* Output appears to have one junk byte... */

    IS_ONE(0, "External LUTs have been loaded for RGB\n",
	      "External LUTs have not been loaded for RGB\n");
    IS_ONE(1, "External LUTs have been loaded for monochrome\n",
	      "External LUTs have not been loaded for monochrome\n");
    printf("End of line time delay: %d\n", (int) rp[2]);
    IS_ONE(4, "Automatic filter wheel operation is disabled\n",
	      "Automatic filter wheel operation is enabled\n");
    IS_ONE(5, "Automatic camera operation is disabled\n",
	      "Automatic camera operation is enabled\n");

    printf("Start address (horiz, vert): %d, %d\n",
	   (int)(short)(((short)rp[12]) << 8) | rp[13],
	   (int)(short)(((short)rp[8]) << 8) | rp[9]);

    printf("Image dimensions (horiz, vert): %d, %d\n",
	   (((int) rp[10]) << 8) | rp[11],
	   (((int) rp[6]) << 8) | rp[7] );

    IS_ONE(15, "Automatic calibration enabled\n",
	       "Automatic calibration disabled\n");
    IS_ONE(16, "End of image warble is disabled\n",
	       "End of image warble is enabled\n");
    IS_ONE(17, "Black jumping is disabled\n",
	       "Black jumping is enabled\n");

    switch ((int)rp[18])
    {
    case 2: 
	printf("Resolution 2K\n");
	break;
    case 4:
	printf("Resolution 4K\n");
	break;
    default:
	printf("Weird resolution: %d\n", (int) rp[18]);
	break;
    }

    IS_ONE(19, "Mode externally selected\n",
	       "Mode is not externally selected\n");
    IS_ONE(20, "90 degree rotation enabled\n",
	       "90 degree rotation disabled\n");
    IS_ONE(21, "Mirror image enabled\n",
	       "Mirror image disabled\n");
    IS_ONE(22, "Unbuffered raster mode enabled\n",
	       "Unbuffered raster mode disabled\n");
    IS_ONE(23, "Return to no filter mode is disabled\n",
	       "Return to no filter mode is enabled\n");
    IS_ONE(24, "Exchange of coords is selected\n",
	       "Exchange of coords is not selected\n");
    IS_ONE(25, "Red-repeat is enabled\n",
	       "Red-repeat is disabled\n");
    printf("Frame number: %d\n", (((int) rp[26]) << 8) | rp[27]);
    IS_ONE(28, "Film is in camera\n",
	       "Film is not in camera\n");
    IS_ONE(29, "Half frame mode enabled\n",
	       "Half frame mode disabled\n");
    IS_ONE(30, "Bulk back in use\n", "" );
    IS_ONE(31, "180 degree rotation enabled\n",
	       "180 degree rotation not enabled\n");
}

/*************************************************************************

  Change the resolution, checking if it's OK first.
*/

int set_resolution( fourK )
int fourK;
{
    int len;
    char ch, *command;

    write_qcr_cmd(RETURN_STATUS);
    len = read(qcr_data,response,BUFSIZ);
    fprintf(stderr," Current resolution mode: %x\n",response[18+1]);
    if (len > 0)
    {				/* Use high-res (4K) (assume 4x5 back) */
	if (fourK)
	{
	    if ( response[18+1] != 4 ) /* +1 for extra junk byte */
	    {
		write_qcr_cmd(RESOLUTION_4K);
		if (verbose_flag)
		    printf("Resetting resolution to 4k, be patient.\n");
		sleep(10);	/* Yow! */
	    }
	}
	else			/* We only need 2k */
	    if ( response[18+1] != 2 )
	    {
		write_qcr_cmd(RESOLUTION_2K);
		if (verbose_flag)
		    printf("Resetting resolution to 2k, be patient.\n");
		sleep(10);	/* Yow! */
	    }
    }
}

/*************************************************************************

  This code sets the location and size of the image on the film.  Needs to
  allow user specifications, as well as perhaps be a little smarter about
  camera module defaults
  Select the resolution, based on the dimensions x_size and y_size.  At the
  moment no provision for command-line placement of the image.  This will
  be added later, as soon as the basics are worked out.  In most cases, 2K
  will be used, so assume that as default.  Check it anyway, to see if it
  needs to be reset.  This resolution selection should compare the current
  film module defaults, rather than hardwiring x_size and y_size test values.
  Also, try using ENABLE_2K and ENABLE_4K (63 and 64) commands, to avoid
  the tedious resetting.

*/

set_up_qcr(x_size,y_size,nslice,offset)
int x_size, y_size, nslice, offset;
{
    int len;
    char ch, *command;
    short scan_start, row_start;
    unsigned char sizes[8];
    int res;

    if ((x_size > 2048) || (y_size > 1536))
	set_resolution( 1 );
    else
	set_resolution( 0 );

    write_qcr_cmd(MIRROR_ENABLE);
    write_qcr_cmd(END_WARBLE_OFF);
    write_qcr_cmd(RED_REPEAT_ON); /* Let's see what happens */

    /* Now that the resolution is settled, center image on film */

    scan_start = (y_size >> 1 ) - offset;
    row_start = 0 - (x_size >> 1 );

    if (verbose_flag)
	fprintf(stderr,"--  centering %d x %d image, at (%d,%d)\n",
		x_size,y_size,row_start,scan_start);

    sizes[0] = (char) (row_start >> 8) & 0xff;
    sizes[1] = (char) row_start;

    sizes[2] = (char) (x_size >> 8) & 0xff;
    sizes[3] = (char) x_size;

    sizes[4] = (char) (scan_start >> 8) & 0xff;
    sizes[5] = (char) scan_start;

    sizes[6] = (char) (nslice >> 8) & 0xff;
    sizes[7] = (char) nslice;

    write_qcr_cmd(SET_DIMENSIONS);
    write(qcr_data,sizes,8);

}

/***************************************************************************

  This is like "set_up_qcr", but does not center the image.  It assumes
  a coordinate range in the upper right quadrant (vs. the centered coords
  the QCR really uses).
*/

set_up_qcr_nc( xstart, ystart, xsize, ysize, fourK )
{
    unsigned char sizes[8];

    set_resolution( fourK );

    write_qcr_cmd(MIRROR_ENABLE);
    write_qcr_cmd(END_WARBLE_OFF);
    write_qcr_cmd(RED_REPEAT_ON); /* Let's see what happens */

    if (fourK)
    {
	xstart = xstart - (4096/2);
	ystart = -ystart + (3072/2);
    }
    else
    {
	xstart = xstart - (2048/2);
	ystart = -ystart + (1536/2);
    }
    
    sizes[0] = (char) (xstart >> 8) & 0xff;
    sizes[1] = (char) xstart & 0xff;

    sizes[2] = (char) (xsize >> 8) & 0xff;
    sizes[3] = (char) xsize;

    sizes[4] = (char) (ystart >> 8) & 0xff;
    sizes[5] = (char) ystart;

    sizes[6] = (char) (ysize >> 8) & 0xff;
    sizes[7] = (char) ysize;

    write_qcr_cmd(SET_DIMENSIONS);
    write(qcr_data,sizes,8);
}

/*
  This routine makes some fatal assumptions, at least with our poor old 237 
  driving it.  A maximum size pixel image, 4K square, would require three
  buffers of 16 Megabytes each.  Don't think that old 68010 box can handle
  such data size.  I'll have to check into this, see what the absolute limit
  is.  Perhaps a means of sending a row at a time, as it is processed rather
  tha all at once?

  Anyway, this routine uses the burst mode, no handshaking.

*/

send_pixel_image(color,buff,len)
int color,len;
char *buff;
{
    char *cl;
    int stat;

    switch(color) {
    case RED:
	cl = "Red";
	write_qcr_cmd(RED_PIXEL);
	break;
    case GREEN:
	cl = "Green";
	write_qcr_cmd(GREEN_PIXEL);
	break;
    case BLUE:
	cl = "Blue";
	write_qcr_cmd(BLUE_PIXEL);
	break;
    default:
	fprintf(stderr,"send_pixel_image: unknown color %d\n",color); break;
    }

    if (verbose_flag)
	fprintf(stderr,"send_pixel_image: %d bytes of %s... ",len,cl);
    /*  stat = write(qcr_data,buff,len); */
    stat = write_data(buff,len);
    qcr_wait_srq();
    if (verbose_flag)
    {
	if (stat > 0)
	    fprintf(stderr,"%d bytes sent.\n",stat);
        else
	    fprintf(stderr," Error sending %s, errno = %d.\n",cl,errno);
    }

    return (stat == len);
}

send_rle_image(color,buff,len)
int color,len;
char *buff;
{
    char *cl;
    int stat;

    switch(color) {
    case RED:
	cl = "Red";
	write_qcr_cmd(RED_RLE);
	break;
    case GREEN:
	cl = "Green";
	write_qcr_cmd(GREEN_RLE);
	break;
    case BLUE:
	cl = "Blue";
	write_qcr_cmd(BLUE_RLE);
	break;
    default:
	fprintf(stderr,"send_rle_image: unknown color %d\n",color); break;
    }

    if (verbose_flag)
	fprintf(stderr,"send_rle_image: %d bytes of %s... ",len,cl);
    stat = write(qcr_data,buff,len);
    qcr_wait_srq();
    if (verbose_flag)
    {
	if (stat > 0)
	    fprintf(stderr,"%d bytes sent.\n",stat);
	else
	    fprintf(stderr," Error sending %s, errno = %d.\n",cl,errno);
    }
    return (stat == len);
}


/***************************************************************************

  Send a complete RLE encoded image in a single buffer.

*/

int
send_3pass_rle(buff,len)
unsigned char *buff;
int len;
{
    int stat;

    write_qcr_cmd(THREE_PASS_RLE);
    if (verbose_flag)
	fprintf(stderr,"send_3passrle_: %d bytes ... ",len);
    write_data(buff,len);
    /*  stat = write(qcr_data,buff,len); */
    if (verbose_flag)
    {   
	if (stat > 0)
	    fprintf(stderr,"%d bytes sent.\n",stat);
        else
	    fprintf(stderr," Error sending data, errno = %d.\n",errno);
    }
    return (stat == len);
}


/***************************************************************************

   Clean up after dumping the image.  This should check for SRQ and error
   conditions, perhaps reset the Matrix to a known state.
*/

close_qcr()
{
     close(qcr_cmd);
     close(qcr_data);
     close(qcr_raw);
  }

/***************************************************************************

 Attempt to reset the device, doesn't seem to always work yet.

*/

int
reset_qcr()
{
    fprintf(stderr," ** Reset QCR ** \n\n");
    hpib_spoll(qcr_raw,3);
    hpib_spoll(qcr_raw,2);
    exit(-1);
}


qcr_rd_brt_tbl( buf )
char buf[];
{
    write_qcr_cmd(RETURN_BRIGHT_TABLE);
    if ( read( qcr_data, buf, BUFSIZ ) <= 0 )
    {
	perror( "Error reading brightness table\n" );
	return 0;
    }
    return buf[0];
}


qcr_rd_brt_lvl( buf )
char buf[];
{
    write_qcr_cmd(RETURN_BRIGHT_LEVELS);
    if ( read( qcr_data, buf, BUFSIZ ) <= 0 )
    {
	perror( "Error reading brightness levels\n" );
	return 0;
    }
    return 1;
}

qcr_ld_lut12( buf )
unsigned char buf[1536];
{
    write_qcr_cmd( LOAD_12_LUT );
    write_data( buf, 3*256*2 );
}

qcr_rd_lut12( buf )
char buf[1536];
{
    write_qcr_cmd( VERIFY_12 );
    if ( read( qcr_data, buf, 1536 ) != 1536 )
    {
	perror( "Error reading 12 bit LUTs\n" );
	return 0;
    }
    return 1;
}

qcr_load_i_luts( lut )
int lut;
{
    char select_lut = lut;
    write_qcr_cmd( LOAD_INTERN_LUT );
    write_data( &select_lut, 1 );
    sleep( 2 );
}
