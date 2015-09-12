#ifndef XLIBINT_H_NOT_AVAILABLE

/* $XConsortium: XGetHClrs.c,v 11.10 88/09/06 16:07:50 martin Exp $ */
/* Copyright    Massachusetts Institute of Technology    1986	*/

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#define NEED_REPLIES
#include <X11/Xlibint.h>

static int counter;
static Status *st;
static Status status;

static int dummy_handle_x_errors( dpy, event)
	Display *dpy;
	XErrorEvent *event;
{
    st[counter++] = 0;
    status = False;
    return 0;
}


Status XAllocColors(dpy, cmap, defs, ndefs, statuses)
register Display *dpy;
Colormap cmap;
XColor *defs;
int ndefs;
Status *statuses;
{
    xAllocColorReply rep;
    register xAllocColorReq *req;
    int (*function)();
    int i;
    Status return_status;

    XSync(dpy, False);

    function = XSetErrorHandler( dummy_handle_x_errors );
    st = statuses;

    LockDisplay(dpy);

    for (i = 0; i < ndefs; i++)
    {
	GetReq(AllocColor, req);

	req->cmap = cmap;
	req->red = defs[i].red;
	req->green = defs[i].green;
	req->blue = defs[i].blue;
    }

    status = True;
    for (counter = 0; counter < ndefs; counter++)
    {
	statuses[counter] = _XReply(dpy, (xReply *) &rep, 0, xTrue);
	if (statuses[counter]) {
	    defs[counter].pixel = rep.pixel;
	    defs[counter].red = rep.red;
	    defs[counter].green = rep.green;
	    defs[counter].blue = rep.blue;
	}
	else 
	    status = False;
    }
    return_status = status;

    UnlockDisplay(dpy);
    SyncHandle();

    XSetErrorHandler( function );
    
    return(return_status);
}
#endif /* !XLIBINT_H_NOT_AVAILABLE */
