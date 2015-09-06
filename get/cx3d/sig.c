/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* sig.c, 7/2/86, T. McCollough, UU */

#include <signal.h>

int sig_handler ( sig, code, scp )
int sig, code;
struct sigcontext *scp;
{
	psignal( sig, "getcx3d" );
	done( );
}

sig_setup ( ) {
	static struct sigvec vec = { sig_handler, 0xffffffff, 0 };

	sigvec( SIGHUP, & vec, 0 );
	sigvec( SIGINT, & vec, 0 );
	sigvec( SIGQUIT, & vec, 0 );
	sigvec( SIGILL, & vec, 0 );
	sigvec( SIGTRAP, & vec, 0 );
	sigvec( SIGIOT, & vec, 0 );
	sigvec( SIGEMT, & vec, 0 );
	sigvec( SIGFPE, & vec, 0 );
	/*sigvec( SIGBUS, & vec, 0 );*/
	sigvec( SIGSEGV, & vec, 0 );
	sigvec( SIGSYS, & vec, 0 );
	sigvec( SIGPIPE, & vec, 0 );
	sigvec( SIGALRM, & vec, 0 );
	sigvec( SIGTERM, & vec, 0 );
	sigvec( SIGURG, & vec, 0 );
	sigvec( SIGCHLD, & vec, 0 );
	sigvec( SIGTTIN, & vec, 0 );
	sigvec( SIGTTOU, & vec, 0 );
	sigvec( SIGIO, & vec, 0 );
	sigvec( SIGXCPU, & vec, 0 );
	sigvec( SIGXFSZ, & vec, 0 );
	sigvec( SIGVTALRM, & vec, 0 );
	sigvec( SIGPROF, & vec, 0 );
	sigvec( SIGWINCH, & vec, 0 );
}

sig_unblock ( ) {
	sigsetmask( 0 );
}

sig_block ( ) {
	sigsetmask( 0xffffffff );
}
