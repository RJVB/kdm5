/* $TOG: error.c /main/17 1998/02/09 13:55:13 kaleb $ */
/* $Id$ */
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
/* $XFree86: xc/programs/xdm/error.c,v 1.2 1998/10/10 15:25:34 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * error.c
 *
 * Log display manager errors to a file as
 * we generally do not have a terminal to talk to
 * or use syslog if it exists
 */

#include "dm.h"
#include "dm_error.h"

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

#define PRINT_QUOTES
#define PRINT_ARRAYS
#define LOG_DEBUG_MASK DEBUG_CORE
#define LOG_PANIC_EXIT 1
#define NEED_ASPRINTF
#define STATIC
#include "printf.c"

void
GDebug (const char *fmt, ...)
{
    va_list args;

    if (debugLevel & DEBUG_HLPCON)
    {
	va_start(args, fmt);
	Logger (DM_DEBUG, fmt, args);
	va_end(args);
    }
}

void
Panic (const char *mesg)
{
    int fd = open ("/dev/console", O_WRONLY);
    write (fd, "xdm panic: ", 11);
    write (fd, mesg, strlen (mesg));
    write (fd, "\n", 1);
#ifdef USE_SYSLOG
    ReInitErrorLog ();
    syslog (LOG_ALERT, "%s", mesg);
#endif
    exit (1);
}

#ifdef USE_SYSLOG
void
ReInitErrorLog ()
{
    InitLog ();
}
#endif

void
InitErrorLog (const char *errorLogFile)
{
    int fd;
    struct stat st;
    char buf[128];

#ifdef USE_SYSLOG
    ReInitErrorLog ();
#endif
    /* We do this independently of using syslog, as we cannot redirect
     * the output of external programs to syslog.
     */
    if (errorLogFile
	|| fstat (1, &st) ||
#ifndef X_NOT_POSIX
	!(S_ISREG(st.st_mode) || S_ISFIFO(st.st_mode))
#else
	!(st.st_mode & (S_IFREG | S_IFIFO))
#endif
	|| fstat (2, &st) ||
#ifndef X_NOT_POSIX
	!(S_ISREG(st.st_mode) || S_ISFIFO(st.st_mode)))
#else
	!(st.st_mode & (S_IFREG | S_IFIFO)))
#endif
    {
	if (!errorLogFile) {
	    sprintf (buf, "/var/log/%s.log", prog);
	    errorLogFile = buf;
	}
	if ((fd = open (errorLogFile, O_CREAT | O_APPEND | O_WRONLY, 0666)) < 0)
	    LogError ("Cannot open log file %s\n", errorLogFile);
	else {
	    if (fd != 1) {
		dup2 (fd, 1);
		close (fd);
	    }
	    dup2 (1, 2);
	}
    }
}

