/* $TOG: socket.c /main/37 1998/02/09 13:56:31 kaleb $ */
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
/* $XFree86: xc/programs/xdm/socket.c,v 3.6 2000/05/11 18:14:43 tsi Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * socket.c - Support for BSD sockets
 */

#if defined(XDMCP) && !defined(STREAMSCONN)

#include "dm.h"
#include "dm_error.h"
#include "dm_socket.h"

#include <netdb.h>

extern int	xdmcpFd;

void
CreateWellKnownSockets (void)
{
    struct sockaddr_in	sock_addr;
    const char *name;

    if (request_port == 0)
	    return;
    Debug ("creating socket %d\n", request_port);
    xdmcpFd = socket (AF_INET, SOCK_DGRAM, 0);
    if (xdmcpFd == -1) {
	LogError ("XDMCP socket creation failed: %s\n", SysErrorMsg());
	return;
    }
    name = localHostname ();
    registerHostname (name, strlen (name));
    /* zero out the entire structure; this avoids 4.4 incompatibilities */
    bzero ((char *) &sock_addr, sizeof (sock_addr));
#ifdef BSD44SOCKETS
    sock_addr.sin_len = sizeof(sock_addr);
#endif
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons ((short) request_port);
    sock_addr.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind (xdmcpFd, (struct sockaddr *)&sock_addr, sizeof (sock_addr)) == -1)
    {
	LogError ("Cannot bind UDP port %d: %s\n", request_port, SysErrorMsg());
	close (xdmcpFd);
	xdmcpFd = -1;
	return;
    }
    RegisterCloseOnFork (xdmcpFd);
    RegisterInput (xdmcpFd);
}

#endif /* !STREAMSCONN && XDMCP */
