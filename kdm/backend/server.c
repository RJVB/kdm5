/* $TOG: server.c /main/21 1998/02/09 13:56:13 kaleb $ */
/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xdm/server.c,v 3.8 2000/05/11 18:14:42 tsi Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * server.c - manage the X server
 */

#include "dm.h"
#include "dm_error.h"
#include "dm_socket.h"

#include <X11/Xlib.h>
#include <X11/Xos.h>

#include <stdio.h>
#include <signal.h>

static int receivedUsr1;

static int serverPause (unsigned t, int serverPid);

Display *dpy;

/* ARGSUSED */
static SIGVAL
CatchUsr1 (int n ATTR_UNUSED)
{
#ifdef SIGNALS_RESET_WHEN_CAUGHT
    (void) Signal (SIGUSR1, CatchUsr1);
#endif
    /* Debug ("display manager caught SIGUSR1\n"); */
    ++receivedUsr1;
}

static int
StartServerOnce (struct display *d)
{
    char	**argv;
    int		pid;

    Debug ("StartServer for %s\n", d->name);
    receivedUsr1 = 0;
    (void) Signal (SIGUSR1, CatchUsr1);
    switch (pid = Fork ()) {
    case 0:
	argv = d->serverArgv;
	if (d->authFile) {
	    argv = addStrArr (argv, "-auth", 5);
	    argv = addStrArr (argv, d->authFile, -1);
	}
	if (!argv) {
	    LogError ("StartServer: no arguments\n");
	    exit (EX_UNMANAGE_DPY);
	}
	Debug ("exec %\"[s\n", argv);
	/*
	 * give the server SIGUSR1 ignored,
	 * it will notice that and send SIGUSR1
	 * when ready
	 */
	(void) Signal (SIGUSR1, SIG_IGN);
	(void) execv (argv[0], argv);
	LogError ("X server %\"s cannot be executed\n", argv[0]);
	sleep ((unsigned) d->openDelay);
	exit (0);
    case -1:
	LogError ("X server fork failed, sleeping\n");
	return 0;
    default:
	break;
    }
    Debug ("X server forked, pid %d\n", pid);
    if (serverPause ((unsigned) d->serverTimeout, pid))
	return FALSE;
    d->serverPid = pid;
    return TRUE;
}

int
StartServer (struct display *d)
{
    int	i;
    int	ret = FALSE;

    i = 0;
    while (d->serverAttempts == 0 || i < d->serverAttempts)
    {
	if ((ret = StartServerOnce (d)) == TRUE)
	    break;
	sleep (d->openDelay);
	i++;
    }
    return ret;
}

/*
 * sleep for t seconds, return 1 if the server is dead when
 * the sleep finishes, 0 else
 */

static Jmp_buf	pauseAbort;
static int	serverPauseRet;

/* ARGSUSED */
static SIGVAL
serverPauseAbort (int n ATTR_UNUSED)
{
    Longjmp (pauseAbort, 1);
}

/* ARGSUSED */
static SIGVAL
serverPauseUsr1 (int n ATTR_UNUSED)
{
    Debug ("display manager paused til SIGUSR1\n");
    ++receivedUsr1;
    Longjmp (pauseAbort, 1);
}

static int
serverPause (unsigned t, int serverPid)
{
    int		pid;

    serverPauseRet = 0;
    if (!Setjmp (pauseAbort)) {
	(void) Signal (SIGALRM, serverPauseAbort);
	(void) Signal (SIGUSR1, serverPauseUsr1);
	if (!receivedUsr1)
	    (void) alarm (t);
	else
#ifdef SYSV
	    (void) alarm ((unsigned) 1);
#else
	    Debug ("already received USR1\n");
#endif
	for (;;) {
#if defined(SYSV) && defined(X_NOT_POSIX)
	    pid = wait ((waitType *) 0);
#else
	    if (!receivedUsr1)
		pid = wait ((waitType *) 0);
	    else
#ifndef X_NOT_POSIX
		pid = waitpid (-1, (int *) 0, WNOHANG);
#else
		pid = wait3 ((waitType *) 0, WNOHANG,
			     (struct rusage *) 0);
#endif /* X_NOT_POSIX */
#endif /* SYSV */
	    if (pid == serverPid ||
		(pid == -1 && errno == ECHILD))
	    {
		Debug ("X server dead\n");
		serverPauseRet = 1;
		break;
	    }
#if !defined(SYSV) || !defined(X_NOT_POSIX)
	    if (pid == 0) {
		Debug ("X server alive and kicking\n");
		break;
	    }
#endif
	}
    }
    (void) alarm ((unsigned) 0);
    (void) Signal (SIGALRM, SIG_DFL);
    (void) Signal (SIGUSR1, CatchUsr1);
    if (serverPauseRet)
	LogError ("X server unexpectedly died\n");
    return serverPauseRet;
}


/*
 * this code is complicated by some TCP failings.  On
 * many systems, the connect will occasionally hang forever,
 * this trouble is avoided by setting up a timeout to Longjmp
 * out of the connect (possibly leaving piles of garbage around
 * inside Xlib) and give up, terminating the server.
 */

static Jmp_buf	openAbort;

/* ARGSUSED */
static SIGVAL
abortOpen (int n ATTR_UNUSED)
{
    Longjmp (openAbort, 1);
}

#ifdef XDMCP

#ifdef STREAMSCONN
#include <tiuser.h>
#endif

static void
GetRemoteAddress (struct display *d, int fd)
{
    char    buf[512];
    int	    len = sizeof (buf);
#ifdef STREAMSCONN
    struct netbuf	netb;
#endif

    XdmcpDisposeARRAY8 (&d->peer);
#ifdef STREAMSCONN
    netb.maxlen = sizeof(buf);
    netb.buf = buf;
    t_getname(fd, &netb, REMOTENAME);
    len = 8;
    /* lucky for us, t_getname returns something that looks like a sockaddr */
#else
    getpeername (fd, (struct sockaddr *) buf, (void *)&len);
#endif
    if (len && XdmcpAllocARRAY8(&d->peer, len))
	memmove( (char *) d->peer.data, buf, len);
    Debug ("got remote address %s %d\n", d->name, d->peer.length);
}

#endif /* XDMCP */

static int
openErrorHandler (Display *dspl ATTR_UNUSED)
{
    LogError ("IO Error in XOpenDisplay\n");
    exit (EX_OPENFAILED_DPY);
    /*NOTREACHED*/
    return(0);
}

int
WaitForServer (struct display *d)
{
    volatile int i;
    /* static int i; */

    i = 0;
    do {
	(void) Signal (SIGALRM, abortOpen);
	(void) alarm ((unsigned) d->openTimeout);
	if (!Setjmp (openAbort)) {
	    Debug ("before XOpenDisplay(%s)\n", d->name);
	    errno = 0;
	    (void) XSetIOErrorHandler (openErrorHandler);
	    dpy = XOpenDisplay (d->name);
#ifdef STREAMSCONN
	    {
		/* For some reason, the next XOpenDisplay we do is
		   going to fail, so we might as well get that out
		   of the way.  There is something broken here. */
		Display *bogusDpy = XOpenDisplay (d->name);
		Debug ("bogus XOpenDisplay %s\n",
		       bogusDpy ? "succeeded" : "failed");
		if (bogusDpy) XCloseDisplay(bogusDpy); /* just in case */
	    }
#endif
	    (void) alarm ((unsigned) 0);
	    (void) Signal (SIGALRM, SIG_DFL);
	    (void) XSetIOErrorHandler ((int (*)(Display *)) 0);
	    Debug ("after XOpenDisplay(%s)\n", d->name);
	    if (dpy) {
#ifdef XDMCP
		if ((d->displayType & d_location) == dForeign)
		    GetRemoteAddress (d, ConnectionNumber (dpy));
#endif
		RegisterCloseOnFork (ConnectionNumber (dpy));
		(void) fcntl (ConnectionNumber (dpy), F_SETFD, 0);
		return 1;
	    }
	    Debug ("OpenDisplay(%s) attempt %d failed: %s\n",
		   d->name, i+1, SysErrorMsg());
	    sleep ((unsigned) d->openDelay);
	} else {
	    LogError ("Hung in XOpenDisplay(%s), aborting\n", d->name);
	    (void) Signal (SIGALRM, SIG_DFL);
	    break;
	}
    } while (++i < d->openRepeat);
    LogError ("Cannot connect to %s, giving up\n", d->name);
    return 0;
}

void
ResetServer (struct display *d)
{
    if (dpy && (d->displayType & d_origin) != dFromXDMCP)
	pseudoReset ();
}

static Jmp_buf	pingTime;

static void
PingLost (void)
{
    Longjmp (pingTime, 1);
}

/* ARGSUSED */
static int
PingLostIOErr (Display *dspl ATTR_UNUSED)
{
    PingLost();
    return 0;
}

/* ARGSUSED */
static SIGVAL
PingLostSig (int n ATTR_UNUSED)
{
    PingLost();
}

int
PingServer (struct display *d)
{
    int	    (*oldError)(Display *);
    SIGVAL  (*oldSig)(int);
    int	    oldAlarm;
    
    oldError = XSetIOErrorHandler (PingLostIOErr);
    oldAlarm = alarm (0);
    oldSig = Signal (SIGALRM, PingLostSig);
    (void) alarm (d->pingTimeout * 60);
    if (!Setjmp (pingTime))
    {
	Debug ("ping X server\n");
	XSync (dpy, 0);
    }
    else
    {
	Debug ("X server dead\n");
	(void) alarm (0);
	(void) Signal (SIGALRM, SIG_DFL);
	XSetIOErrorHandler (oldError);
	return 0;
    }
    (void) alarm (0);
    (void) Signal (SIGALRM, oldSig);
    (void) alarm (oldAlarm);
    Debug ("X server alive\n");
    XSetIOErrorHandler (oldError);
    return 1;
}
