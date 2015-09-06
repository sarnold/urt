/* 
 * bread.c - 
 * 
 * Author:	Todd W. Fuqua
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Jul 25 1984
 * Copyright (c) 1984 Todd W. Fuqua
 * 
 * $Header: bread.c,v 2.3 86/06/18 11:46:19 thomas Exp $
 * $Log:	bread.c,v $
 * Revision 2.3  86/06/18  11:46:19  thomas
 * Add vax ifdef.
 * 
 * Revision 2.2  85/04/26  15:03:56  thomas
 * Lint and potential bug fixes.
 * 
 * Revision 2.1  85/03/05  16:00:22  thomas
 * *** empty log message ***
 * 
 */
static char rcs_ident[] = "$Header: bread.c,v 2.3 86/06/18 11:46:19 thomas Exp $";


#include "stdio.h"
bread(ptr, size, iop)
unsigned size;
register char *ptr;
register FILE *iop;
{
	register unsigned ndone, s;
	register int c;
	
	if (!size) return 0;
	
	ndone = size;
	if ( iop->_cnt >= ndone )
	{
	    switch ( ndone )
	    {
	    /* WARNING -- the first two cases should be
	     * "sizeof(short)" and "sizeof(long)", but the compiler
	     * doesn't like those.
	     */
	    case sizeof(short):	*(short *)ptr = *(short *)iop->_ptr;
				break;
	    case sizeof(long):	*(long *)ptr = *(int *)iop->_ptr;
				break;
	    default:		/*bcopy( iop->_ptr, ptr, ndone );*/
#ifdef vax
				asm("	movl  4(r10),r0");
				asm("	movc3 r9,(r0),(r11)");
#else
				bcopy( iop->_ptr, ptr, ndone );
#endif
	    }
	    iop->_cnt -= ndone;
	    iop->_ptr += ndone;
	}
	else
	    while ( ndone != 0 )
	    {
		if ( iop->_cnt == 0 )
		    if ((c = _filbuf(iop)) == EOF)
			return (size - ndone) ;
		    else
		    {
			*ptr++ = c;
			ndone--;
		    }

		if (s = ndone)
		{
		    if ( s > iop->_cnt )
			s = iop->_cnt;
		    iop->_cnt -= s;	/* work-around for CC bug */
/*		    bcopy( iop->_ptr, ptr, s );*/
		    asm("	movl  4(r10),r0");
		    asm("	movc3 r8,(r0),(r11)");
		    iop->_ptr += s;
		    ndone -= s;
		    ptr += s;
		}
	    }

	return size;
}

