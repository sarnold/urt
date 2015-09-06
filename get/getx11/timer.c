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

/* Copyright 1990,1992 The Regents of the University of Michigan */

#ifndef lint
static char rcsid[] = "$Header: /l/spencer/src/urt/get/getx11/RCS/timer.c,v 3.0.1.3 1992/03/04 19:31:40 spencer Exp $";
#endif

#include "rle_config.h"
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

#define USPS    1000000         /* number of microseconds in a second */
#define TICK    10000           /* system clock resolution in microseconds */

#ifndef NO_ITIMER
static int ringring;
static void (*ofunc)();
    
static void
sleepx()
{
        ringring = 1;
}
#endif

void
set_timer(n)
unsigned n;
{
#ifndef	NO_ITIMER
    struct itimerval itv;
    register struct itimerval *itp = &itv;
    if (n == 0)
    {
	ringring = 1;
	return;
    }
    timerclear(&itp->it_interval);
    itp->it_value.tv_sec = n / USPS;
    itp->it_value.tv_usec = n % USPS;
    ofunc = (void (*)())signal(SIGALRM, sleepx);

    ringring = 0;
    (void) setitimer(ITIMER_REAL, itp, (struct itimerval *)0);
#endif
}

#ifndef sigmask
#define sigmask(m)	(1 << ((m)-1))
#endif

void
wait_timer()
{
#ifndef NO_ITIMER
    while (!ringring)
	sigpause( ~sigmask(SIGALRM));
    signal(SIGALRM, ofunc);
#endif
}
