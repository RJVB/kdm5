/* $TOG: auth.c /main/64 1998/02/26 10:02:22 barstow $ */
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
/* $XFree86: xc/programs/xdm/auth.c,v 3.20 1999/12/27 00:40:08 robin Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * auth.c
 *
 * maintain the authorization generation daemon
 */

#include "dm.h"
#include "dm_auth.h"
#include "dm_error.h"

#include <X11/X.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/ioctl.h>

#if defined(TCPCONN) || defined(STREAMSCONN)
# include "dm_socket.h"
#endif
#ifdef DNETCONN
# include <netdnet/dn.h>
# include <netdnet/dnetdb.h>
#endif

#if (defined(_POSIX_SOURCE) && !defined(AIXV3) && !defined(__QNX__)) || defined(hpux) || defined(USG) || defined(SVR4) || (defined(SYSV) && defined(i386))
# define NEED_UTSNAME
# include <sys/utsname.h>
#endif

#if defined(SYSV) && defined(i386)
# include <sys/stream.h>
# ifdef ISC
#  include <stropts.h>
#  include <sys/sioctl.h>
# endif /* ISC */
#endif /* i386 */

#ifdef SVR4
# include <netdb.h>
# ifndef SCO325
#  include <sys/sockio.h>
# endif
# include <sys/stropts.h>
#endif
#ifdef __convex__
# include <sync/queue.h>
# include <sync/sema.h>
#endif
#ifdef __GNU__
# include <netdb.h>
# undef SIOCGIFCONF
#else /* __GNU__ */
# include <net/if.h>
#endif /* __GNU__ */

#ifdef __EMX__
# define link rename
int chown(int a,int b,int c) {}
# include <io.h>
#endif

struct AuthProtocol {
    unsigned short  name_length;
    const char	    *name;
    void	    (*InitAuth)(unsigned short len, const char *name);
    Xauth	    *(*GetAuth)(unsigned short len, const char *name);
#ifdef XDMCP
    void	    (*GetXdmcpAuth)(
    			struct protoDisplay	*pdpy,
    			unsigned short		authorizationNameLen,
    			const char		*authorizationName);
#endif
    int		    inited;
};

#ifdef XDMCP
# define xdmcpauth(arg) , arg
#else
# define xdmcpauth(arg)
#endif

static struct AuthProtocol AuthProtocols[] = {
{ (unsigned short) 18,	"MIT-MAGIC-COOKIE-1",
    MitInitAuth, MitGetAuth xdmcpauth(NULL), 0
},
#ifdef HASXDMAUTH
{ (unsigned short) 19,	"XDM-AUTHORIZATION-1",
    XdmInitAuth, XdmGetAuth xdmcpauth(XdmGetXdmcpAuth), 0
},
#endif
#ifdef SECURE_RPC
{ (unsigned short) 9, "SUN-DES-1",
    SecureRPCInitAuth, SecureRPCGetAuth xdmcpauth(NULL), 0
},
#endif
#ifdef K5AUTH
{ (unsigned short) 14, "MIT-KERBEROS-5",
    Krb5InitAuth, Krb5GetAuth xdmcpauth(NULL), 0
},
#endif
};

static struct AuthProtocol *
findProtocol (unsigned short name_length, const char *name)
{
    unsigned	i;

    for (i = 0; i < as(AuthProtocols); i++)
	if (AuthProtocols[i].name_length == name_length &&
	    memcmp(AuthProtocols[i].name, name, name_length) == 0)
	{
	    return &AuthProtocols[i];
	}
    return (struct AuthProtocol *) 0;
}

int
ValidAuthorization (unsigned short name_length, const char *name)
{
    if (findProtocol (name_length, name))
	return TRUE;
    return FALSE;
}

static Xauth *
GenerateAuthorization (unsigned short name_length, const char *name)
{
    struct AuthProtocol	*a;
    Xauth   *auth = 0;

    Debug ("GenerateAuthorization %s\n", name);
    if ((a = findProtocol (name_length, name)))
    {
	if (!a->inited)
	{
	    (*a->InitAuth) (name_length, name);
	    a->inited = TRUE;
	}
	auth = (*a->GetAuth) (name_length, name);
	if (auth)
	{
	    Debug ("Got %p (%d %.*s) %02[*hhx\n", auth,
		    auth->name_length, auth->name_length, auth->name,
		    auth->data_length, auth->data);
	}
	else
	    Debug ("Got (null)\n");
    }
    else
    {
	Debug ("Unknown authorization %s\n", name);
    }
    return auth;
}

#ifdef XDMCP

void
SetProtoDisplayAuthorization (
    struct protoDisplay	*pdpy,
    unsigned short	authorizationNameLen,
    const char		*authorizationName)
{
    struct AuthProtocol	*a;
    Xauth   *auth;

    a = findProtocol (authorizationNameLen, authorizationName);
    pdpy->xdmcpAuthorization = pdpy->fileAuthorization = 0;
    if (a)
    {
	if (!a->inited)
	{
	    (*a->InitAuth) (authorizationNameLen, authorizationName);
	    a->inited = TRUE;
	}
	if (a->GetXdmcpAuth)
	{
	    (*a->GetXdmcpAuth) (pdpy, authorizationNameLen, authorizationName);
	    auth = pdpy->xdmcpAuthorization;
	}
	else
	{
	    auth = (*a->GetAuth) (authorizationNameLen, authorizationName);
	    pdpy->fileAuthorization = auth;
	    pdpy->xdmcpAuthorization = 0;
	}
	if (auth)
	    Debug ("Got %p (%d %.*s)\n", auth,
		auth->name_length, auth->name_length, auth->name);
	else
	    Debug ("Got (null)\n");
    }
}

#endif /* XDMCP */

void
CleanUpFileName (const char *src, char *dst, int len)
{
    while (*src) {
	if (--len <= 0)
		break;
	switch (*src & 0x7f)
	{
	case '/':
	    *dst++ = '_';
	    break;
	case '-':
	    *dst++ = '.';
	    break;
	default:
	    *dst++ = (*src & 0x7f);
	}
	++src;
    }
    *dst = '\0';
}


static FILE *
fdOpenW (int fd)
{
    FILE *f;

    if (fd >= 0) {
	if ((f = fdopen (fd, "w")))
	    return f;
	close (fd);
    }
    return 0;
}


#ifdef SYSV
# define NAMELEN	14
#else
# define NAMELEN	255
#endif

static FILE *
MakeServerAuthFile (struct display *d)
{
    FILE	*f;
#ifndef HAS_MKSTEMP
    int		r;
#endif
    char	cleanname[NAMELEN], nambuf[NAMELEN+128];

    /*
     * Some paranoid, but still not sufficient (DoS was still possible)
     * checks used to be here. I removed all this stuff because
     * a) authDir is supposed to be /var/run/xauth (=safe) or similar and
     * b) even if it's not (say, /tmp), we create files safely (hopefully).
     */
    if (mkdir(authDir, 0755) < 0  &&  errno != EEXIST)
	return 0;
    CleanUpFileName (d->name, cleanname, NAMELEN - 8);
#ifdef HAS_MKSTEMP
    sprintf (nambuf, "%s/A%s-XXXXXX", authDir, cleanname);
    if ((f = fdOpenW (mkstemp (nambuf)))) {
	StrDup (&d->authFile, nambuf);
	return f;
    }
#else
    for (r = 0; r < 100; r++) {
	sprintf (nambuf, "%s/A%s-XXXXXX", authDir, cleanname);
	(void) mktemp (nambuf);
	if ((f = fdOpenW (open (nambuf, O_WRONLY | O_CREAT | O_EXCL, 0600)))) {
	    StrDup (&d->authFile, nambuf);
	    return f;
	}
    }
#endif	
    return 0;
}

int
SaveServerAuthorizations (
    struct display  *d,
    Xauth	    **auths,
    int		    count)
{
    FILE	*auth_file;
    int		i, ret;

    if (!d->authFile && d->clientAuthFile && *d->clientAuthFile)
	StrDup (&d->authFile, d->clientAuthFile);
    if (d->authFile) {
	if (!(auth_file = fopen (d->authFile, "w"))) {
	    LogError ("Cannot open server authorization file %s\n", d->authFile);
	    free (d->authFile);
	    d->authFile = NULL;
	    return FALSE;
	}
    } else {
	if (!(auth_file = MakeServerAuthFile (d))) {
	    LogError ("Cannot create server authorization file\n");
	    return FALSE;
	}
    }
    Debug ("File: %s auth: %p\n", d->authFile, auths);
    ret = TRUE;
    for (i = 0; i < count; i++)
    {
	/*
	 * User-based auths may not have data until
	 * a user logs in.  In which case don't write
	 * to the auth file so xrdb and setup programs don't fail.
	 */
	if (auths[i]->data_length > 0)
	    if (!XauWriteAuth (auth_file, auths[i]) ||
		fflush (auth_file) == EOF)
	    {
		LogError ("Cannot write server authorization file %s\n",
			  d->authFile);
		ret = FALSE;
		free (d->authFile);
		d->authFile = NULL;
	    }
    }
    fclose (auth_file);
    return ret;
}

void
SetLocalAuthorization (struct display *d)
{
    Xauth	*auth, **auths;
    int		i, j;

    if (d->authorizations)
    {
	for (i = 0; i < d->authNum; i++)
	    XauDisposeAuth (d->authorizations[i]);
	free ((char *) d->authorizations);
	d->authorizations = (Xauth **) NULL;
	d->authNum = 0;
    }
    Debug ("SetLocalAuthorization %s, auths %[s\n", d->name, d->authNames);
    if (!d->authNames)
	return;
    for (i = 0; d->authNames[i]; i++)
	;
    d->authNameNum = i;
    if (d->authNameLens)
	free ((char *) d->authNameLens);
    d->authNameLens = (unsigned short *) malloc
				(d->authNameNum * sizeof (unsigned short));
    if (!d->authNameLens)
	return;
    for (i = 0; i < d->authNameNum; i++)
	d->authNameLens[i] = strlen (d->authNames[i]);
    auths = (Xauth **) malloc (d->authNameNum * sizeof (Xauth *));
    if (!auths)
	return;
    j = 0;
    for (i = 0; i < d->authNameNum; i++)
    {
	auth = GenerateAuthorization (d->authNameLens[i], d->authNames[i]);
	if (auth)
	    auths[j++] = auth;
    }
    if (SaveServerAuthorizations (d, auths, j))
    {
	d->authorizations = auths;
	d->authNum = j;
    }
    else
    {
	for (i = 0; i < j; i++)
	    XauDisposeAuth (auths[i]);
	free ((char *) auths);
    }
}

/*
 * Set the authorization to use for xdm's initial connection
 * to the X server.  Cannot use user-based authorizations
 * because no one has logged in yet, so we don't have any
 * user credentials.
 * Well, actually we could use SUN-DES-1 because we tell the server
 * to allow root in.  This is bogus and should be fixed.
 */
void
SetAuthorization (struct display *d)
{
    register Xauth **auth = d->authorizations;
    int i;

    for (i = 0; i < d->authNum; i++)
    {
	if (auth[i]->name_length == 9 &&
	    memcmp(auth[i]->name, "SUN-DES-1", 9) == 0)
	    continue;
	if (auth[i]->name_length == 14 &&
	    memcmp(auth[i]->name, "MIT-KERBEROS-5", 14) == 0)
	    continue;
	XSetAuthorization (auth[i]->name, (int) auth[i]->name_length,
			   auth[i]->data, (int) auth[i]->data_length);
    }
}

static int
openFiles (const char *name, char *new_name, FILE **oldp, FILE **newp)
{
	strcat (strcpy (new_name, name), "-n");
	if (!(*newp = 
	      fdOpenW (open (new_name, O_WRONLY | O_CREAT | O_TRUNC, 0600)))) {
		Debug ("can't open new file %s\n", new_name);
		return 0;
	}
	*oldp = fopen (name, "r");
	Debug ("opens succeeded %s %s\n", name, new_name);
	return 1;
}

struct addrList {
	unsigned short	family;
	unsigned short	address_length;
	char	*address;
	unsigned short	number_length;
	char	*number;
	unsigned short	name_length;
	char	*name;
	struct addrList	*next;
};

static struct addrList	*addrs;

static void
initAddrs (void)
{
	addrs = 0;
}

static void
doneAddrs (void)
{
	struct addrList	*a, *n;
	for (a = addrs; a; a = n) {
		n = a->next;
		if (a->address)
			free (a->address);
		if (a->number)
			free (a->number);
		free ((char *) a);
	}
}

static int checkEntry (Xauth *auth);

static void
saveEntry (Xauth *auth)
{
	struct addrList	*new;

	new = (struct addrList *) malloc (sizeof (struct addrList));
	if (!new) {
		LogOutOfMem ("saveEntry");
		return;
	}
	if ((new->address_length = auth->address_length) > 0) {
		new->address = malloc (auth->address_length);
		if (!new->address) {
			LogOutOfMem ("saveEntry");
			free ((char *) new);
			return;
		}
		memmove( new->address, auth->address, (int) auth->address_length);
	} else
		new->address = 0;
	if ((new->number_length = auth->number_length) > 0) {
		new->number = malloc (auth->number_length);
		if (!new->number) {
			LogOutOfMem ("saveEntry");
			free (new->address);
			free ((char *) new);
			return;
		}
		memmove( new->number, auth->number, (int) auth->number_length);
	} else
		new->number = 0;
	if ((new->name_length = auth->name_length) > 0) {
		new->name = malloc (auth->name_length);
		if (!new->name) {
			LogOutOfMem ("saveEntry");
			free (new->number);
			free (new->address);
			free ((char *) new);
			return;
		}
		memmove( new->name, auth->name, (int) auth->name_length);
	} else
		new->name = 0;
	new->family = auth->family;
	new->next = addrs;
	addrs = new;
}

static int
checkEntry (Xauth *auth)
{
	struct addrList	*a;

	for (a = addrs; a; a = a->next) {
		if (a->family == auth->family &&
		    a->address_length == auth->address_length &&
 		    !memcmp (a->address, auth->address, auth->address_length) &&
		    a->number_length == auth->number_length &&
 		    !memcmp (a->number, auth->number, auth->number_length) &&
		    a->name_length == auth->name_length &&
		    !memcmp (a->name, auth->name, auth->name_length))
		{
			return 1;
		}
	}
	return 0;
}

static int  doWrite;

static void
writeAuth (FILE *file, Xauth *auth)
{
    if (debugLevel & DEBUG_AUTH) {	/* normally too verbose */
	Debug (	"writeAuth: doWrite = %d\n"
		"family: %d\n"
		"addr:   %02[*:hhx\n"
		"number: %02[*:hhx\n"
		"name:   %02[*:hhx\n"
		"data:   %02[*:hhx\n", 
		doWrite, auth->family,
		auth->address_length, auth->address,
		auth->number_length, auth->number,
		auth->name_length, auth->name,
		auth->data_length, auth->data);
    }
    if (doWrite)
	XauWriteAuth (file, auth);
}

static void
writeAddr (
    int		family,
    int		addr_length,
    char	*addr,
    FILE	*file,
    Xauth	*auth)
{
	auth->family = (unsigned short) family;
	auth->address_length = addr_length;
	auth->address = addr;
	Debug ("writeAddr: writing and saving an entry\n");
	writeAuth (file, auth);
	saveEntry (auth);
}

static void
DefineLocal (FILE *file, Xauth *auth)
{
	char	displayname[100];
	char	tmp_displayname[100];

	tmp_displayname[0] = 0;

	/* stolen from xinit.c */

/* Make sure this produces the same string as _XGetHostname in lib/X/XlibInt.c.
 * Otherwise, Xau will not be able to find your cookies in the Xauthority file.
 *
 * Note: POSIX says that the ``nodename'' member of utsname does _not_ have
 *       to have sufficient information for interfacing to the network,
 *       and so, you may be better off using gethostname (if it exists).
 */

#ifdef NEED_UTSNAME

	/* hpux:
	 * Why not use gethostname()?  Well, at least on my system, I've had to
	 * make an ugly kernel patch to get a name longer than 8 characters, and
	 * uname() lets me access to the whole string (it smashes release, you
	 * see), whereas gethostname() kindly truncates it for me.
	 */
	{
	struct utsname name;

	uname(&name);
	strcpy(displayname, name.nodename);
	}
	writeAddr (FamilyLocal, strlen (displayname), displayname, file, auth);

	strcpy(tmp_displayname, displayname);
#endif

#if (!defined(NEED_UTSNAME) || defined (hpux))
        /* AIXV3:
	 * In AIXV3, _POSIX_SOURCE is defined, but uname gives only first
	 * field of hostname. Thus, we use gethostname instead.
	 */

	/*
	 * For HP-UX, HP's Xlib expects a fully-qualified domain name, which
	 * is achieved by using gethostname().  For compatability, we must
	 * also still create the entry using uname() above.
	 */
	gethostname(displayname, sizeof(displayname));

	/*
	 * If gethostname and uname both returned the same name,
	 * do not write a duplicate entry.
	 */
	if (strcmp (displayname, tmp_displayname))
	    writeAddr (FamilyLocal, strlen (displayname), displayname, 
		       file, auth);
#endif
}

/* Argh! this is evil. But ConvertAddr works only with Xdmcp.h */
#ifdef XDMCP

/*
 * Call ConvertAddr(), and if it returns an IPv4 localhost, convert it
 * to a local display name.  Meets the _XTransConvertAddress's localhost
 * hack.
 */
 
static int
ConvertAuthAddr (XdmcpNetaddr saddr, int *len, char **addr)
{
    int ret = ConvertAddr(saddr, len, addr);
    if (ret == FamilyInternet &&
	((struct in_addr *)*addr)->s_addr == htonl(0x7F000001L))
	ret = FamilyLocal;
    return ret;
}

#ifdef SYSV_SIOCGIFCONF

/* Deal with different SIOCGIFCONF ioctl semantics on SYSV, SVR4 */

int
ifioctl (int fd, int cmd, char *arg)
{
    struct strioctl ioc;
    int ret;

    bzero((char *) &ioc, sizeof(ioc));
    ioc.ic_cmd = cmd;
    ioc.ic_timout = 0;
    if (cmd == SIOCGIFCONF)
    {
	ioc.ic_len = ((struct ifconf *) arg)->ifc_len;
	ioc.ic_dp = ((struct ifconf *) arg)->ifc_buf;
#ifdef ISC
	/* SIOCGIFCONF is somewhat brain damaged on ISC. The argument
	 * buffer must contain the ifconf structure as header. Ifc_req
	 * is also not a pointer but a one element array of ifreq
	 * structures. On return this array is extended by enough
	 * ifreq fields to hold all interfaces. The return buffer length
	 * is placed in the buffer header.
	 */
        ((struct ifconf *) ioc.ic_dp)->ifc_len =
                                         ioc.ic_len - sizeof(struct ifconf);
#endif
    }
    else
    {
	ioc.ic_len = sizeof(struct ifreq);
	ioc.ic_dp = arg;
    }
    ret = ioctl(fd, I_STR, (char *) &ioc);
    if (ret >= 0 && cmd == SIOCGIFCONF)
#ifdef SVR4
	((struct ifconf *) arg)->ifc_len = ioc.ic_len;
#endif
#ifdef ISC
    {
	((struct ifconf *) arg)->ifc_len =
				 ((struct ifconf *)ioc.ic_dp)->ifc_len;
	((struct ifconf *) arg)->ifc_buf = 
			(caddr_t)((struct ifconf *)ioc.ic_dp)->ifc_req;
    }
#endif
    return(ret);
}
#endif /* SYSV_SIOCGIFCONF */

#if defined(STREAMSCONN) && !defined(SYSV_SIOCGIFCONF) && !defined(NCR)

#include <tiuser.h>

/* Define this host for access control.  Find all the hosts the OS knows about 
 * for this fd and add them to the selfhosts list.
 * TLI version, written without sufficient documentation.
 */
static void
DefineSelf (int fd, FILE *file, Xauth *auth)
{
    struct netbuf	netb;
    char		addrret[1024]; /* easier than t_alloc */
    
    netb.maxlen = sizeof(addrret);
    netb.buf = addrret;
    if (t_getname (fd, &netb, LOCALNAME) == -1)
	t_error ("t_getname");
    /* what a kludge */
    writeAddr (FamilyInternet, 4, netb.buf+4, file, auth);
}

#else

#ifdef WINTCP /* NCR with Wollongong TCP */

#include <sys/un.h>
#include <stropts.h>
#include <tiuser.h>

#include <sys/stream.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

static void
DefineSelf (int fd, FILE *file, Xauth *auth)
{
    /*
     * The Wollongong drivers used by NCR SVR4/MP-RAS don't understand the
     * socket IO calls that most other drivers seem to like. Because of
     * this, this routine must be special cased for NCR. Eventually,
     * this will be cleared up.
     */

    struct ipb ifnet;
    struct in_ifaddr ifaddr;
    struct strioctl str;
    unsigned char *addr;
    int	len, ipfd;

    if ((ipfd = open ("/dev/ip", O_RDWR, 0 )) < 0) {
	LogError ("Getting interface configuration\n");
	return;
    }

    /* Indicate that we want to start at the begining */
    ifnet.ib_next = (struct ipb *) 1;

    while (ifnet.ib_next)
    {
	str.ic_cmd = IPIOC_GETIPB;
	str.ic_timout = 0;
	str.ic_len = sizeof (struct ipb);
	str.ic_dp = (char *) &ifnet;

	if (ioctl (ipfd, (int) I_STR, (char *) &str) < 0)
	{
	    close (ipfd);
	    LogError ("Getting interface configuration\n");
	    return;
	}

	ifaddr.ia_next = (struct in_ifaddr *) ifnet.if_addrlist;
	str.ic_cmd = IPIOC_GETINADDR;
	str.ic_timout = 0;
	str.ic_len = sizeof (struct in_ifaddr);
	str.ic_dp = (char *) &ifaddr;

	if (ioctl (ipfd, (int) I_STR, (char *) &str) < 0)
	{
	    close (ipfd);
	    LogError ("Getting interface configuration\n");
	    return;
	}

	/*
	 * Ignore the 127.0.0.1 entry.
	 */
	if (IA_SIN(&ifaddr)->sin_addr.s_addr == htonl(0x7f000001) )
		continue;

	writeAddr (FamilyInternet, 4, (char *)&(IA_SIN(&ifaddr)->sin_addr), file, auth);
 
    }
    close(ipfd);
}

#else /* WINTCP */

#ifdef SIOCGIFCONF

/* Define this host for access control.  Find all the hosts the OS knows about 
 * for this fd and add them to the selfhosts list.
 */
static void
DefineSelf (int fd, FILE *file, Xauth *auth)
{
    char		buf[2048], *cp, *cplim;
    struct ifconf	ifc;
    int 		len;
    char 		*addr;
    int 		family;
    register struct ifreq *ifr;
    
    ifc.ifc_len = sizeof (buf);
    ifc.ifc_buf = buf;
    if (ifioctl (fd, SIOCGIFCONF, (char *) &ifc) < 0) {
	LogError ("Trouble getting network interface configuration\n");
	return;
    }

    cplim = (char *) IFC_IFC_REQ (ifc) + ifc.ifc_len;

    for (cp = (char *) IFC_IFC_REQ (ifc); cp < cplim; cp += ifr_size (ifr))
    {
	ifr = (struct ifreq *) cp;
#ifdef DNETCONN
	/*
	 * this is ugly but SIOCGIFCONF returns decnet addresses in
	 * a different form from other decnet calls
	 */
	if (ifr->ifr_addr.sa_family == AF_DECnet) {
		len = sizeof (struct dn_naddr);
		addr = (char *)ifr->ifr_addr.sa_data;
		family = FamilyDECnet;
	} else
#endif
	{
	    if (ConvertAddr ((XdmcpNetaddr) &ifr->ifr_addr, &len, &addr) < 0)
		continue;
	    if (len == 0)
 	    {
		Debug ("Skipping zero length address\n");
		continue;
	    }
	    /*
	     * don't write out 'localhost' entries, as
	     * they may conflict with other local entries.
	     * DefineLocal will always be called to add
	     * the local entry anyway, so this one can
	     * be tossed.
	     */
	    if (len == 4 &&
		addr[0] == 127 && addr[1] == 0 &&
		addr[2] == 0 && addr[3] == 1)
	    {
		    Debug ("Skipping localhost address\n");
		    continue;
	    }
	    family = FamilyInternet;
	}
	Debug ("DefineSelf: write network address, length %d\n", len);
	writeAddr (family, len, addr, file, auth);
    }
}

#else /* SIOCGIFCONF */

/* Define this host for access control.  Find all the hosts the OS knows about 
 * for this fd and add them to the selfhosts list.
 */
static void
DefineSelf (int fd, int file, int auth)
{
    int		len;
    caddr_t	addr;
    int		family;

    struct utsname name;
    register struct hostent  *hp;

    union {
	struct  sockaddr   sa;
	struct  sockaddr_in  in;
    } saddr;
	
    struct	sockaddr_in	*inetaddr;

    /* hpux:
     * Why not use gethostname()?  Well, at least on my system, I've had to
     * make an ugly kernel patch to get a name longer than 8 characters, and
     * uname() lets me access to the whole string (it smashes release, you
     * see), whereas gethostname() kindly truncates it for me.
     */
    uname(&name);
    if ((hp = gethostbyname (name.nodename))) {
	saddr.sa.sa_family = hp->h_addrtype;
	inetaddr = (struct sockaddr_in *) (&(saddr.sa));
	memmove( (char *) &(inetaddr->sin_addr), (char *) hp->h_addr, (int) hp->h_length);
	if ( (family = ConvertAddr ( &(saddr.sa), &len, &addr)) >= 0) {
	    writeAddr (FamilyInternet, sizeof (inetaddr->sin_addr),
			(char *) (&inetaddr->sin_addr), file, auth);
	}
    }
}

#endif /* SIOCGIFCONF else */
#endif /* WINTCP else */
#endif /* STREAMSCONN && !SYSV_SIOCGIFCONF else */

#endif /* XDMCP */

static void
setAuthNumber (Xauth *auth, const char *name)
{
    char	*colon;
    char	*dot;

    Debug ("setAuthNumber %s\n", name);
    colon = strrchr(name, ':');
    if (colon) {
	++colon;
	dot = strchr(colon, '.');
	if (dot)
	    auth->number_length = dot - colon;
	else
	    auth->number_length = strlen (colon);
	if (!StrNDup (&auth->number, colon, auth->number_length)) {
	    LogOutOfMem ("setAuthNumber");
	    auth->number_length = 0;
	}
	Debug ("setAuthNumber: %s\n", auth->number);
    }
}

static void
writeLocalAuth (FILE *file, Xauth *auth, const char *name)
{
#ifdef XDMCP
    int	fd;
#endif

    Debug ("writeLocalAuth: %s %.*s\n", name, auth->name_length, auth->name);
    setAuthNumber (auth, name);
#ifdef XDMCP
#ifdef STREAMSCONN
    fd = t_open ("/dev/tcp", O_RDWR, 0);
    t_bind(fd, NULL, NULL);
    DefineSelf (fd, file, auth);
    t_unbind (fd);
    t_close (fd);
#endif
#ifdef TCPCONN
    fd = socket (AF_INET, SOCK_STREAM, 0);
    DefineSelf (fd, file, auth);
    close (fd);
#endif
#ifdef DNETCONN
    fd = socket (AF_DECnet, SOCK_STREAM, 0);
    DefineSelf (fd, file, auth);
    close (fd);
#endif
#endif /* XDMCP */
    DefineLocal (file, auth);
}

#ifdef XDMCP

static void
writeRemoteAuth (FILE *file, Xauth *auth, XdmcpNetaddr peer, int peerlen, const char *name)
{
    int	    family = FamilyLocal;
    char    *addr;
    
    Debug ("writeRemoteAuth: %s %.*s\n", name, auth->name_length, auth->name);
    if (!peer || peerlen < 2)
	return;
    setAuthNumber (auth, name);
    family = ConvertAuthAddr (peer, &peerlen, &addr);
    Debug ("writeRemoteAuth: family %d\n", family);
    if (family != FamilyLocal)
    {
	Debug ("writeRemoteAuth: %d, %02[*:hhx\n",
		family, peerlen, addr);
	writeAddr (family, peerlen, addr, file, auth);
    }
    else
    {
	writeLocalAuth (file, auth, name);
    }
}

#endif /* XDMCP */

static void
startUserAuth (struct verify_info *verify, char *buf, char *nbuf, 
	       FILE **old, FILE **new)
{
    const char	*home;
    int		lockStatus;

    initAddrs ();
    *new = 0;
    if ((home = getEnv (verify->userEnviron, "HOME"))) {
	strcpy (buf, home);
	if (home[strlen(home) - 1] != '/')
	    strcat (buf, "/");
	strcat (buf, ".Xauthority");
	Debug ("XauLockAuth %s\n", buf);
	lockStatus = XauLockAuth (buf, 1, 2, 10);
	Debug ("Lock is %d\n", lockStatus);
	if (lockStatus == LOCK_SUCCESS)
	    if (!openFiles (buf, nbuf, old, new))
		XauUnlockAuth (buf);
    }
    if (!*new)
	LogInfo ("can't update authorization file in home dir %s\n", home);
}

static void
endUserAuth (FILE *old, FILE *new, const char *nname)
{
    Xauth	*entry;
    struct stat	statb;

    if (old) {
	if (fstat (fileno (old), &statb) != -1)
	    chmod (nname, (int) (statb.st_mode & 0777));
	/*SUPPRESS 560*/
	while ((entry = XauReadAuth (old))) {
	    if (!checkEntry (entry))
	    {
		Debug ("Writing an entry\n");
		writeAuth (new, entry);
	    }
	    XauDisposeAuth (entry);
	}
	fclose (old);
    }
    fclose (new);
    doneAddrs ();
}

static char *
moveUserAuth (const char *name, char *new_name, char *envname)
{
    if (unlink (name))
	Debug ("unlink %s failed\n", name);
    if (link (new_name, name)) {
	Debug ("link failed %s %s\n", new_name, name);
	LogError ("Can't move authorization into place\n");
	envname = new_name;
    } else {
	Debug ("new authorization moved into place\n");
	unlink (new_name);
    }
    XauUnlockAuth (name);
    return envname;
}

void
SetUserAuthorization (struct display *d, struct verify_info *verify)
{
    FILE	*old, *new;
    char	*name;
    char	*envname;
    Xauth	**auths;
    int		i;
    int		magicCookie;
    int		data_len;
    char	name_buf[1024], new_name[1024];

    Debug ("SetUserAuthorization\n");
    auths = d->authorizations;
    if (auths) {
	startUserAuth (verify, name_buf, new_name, &old, &new);
	if (new) {
	    envname = 0;
	    name = name_buf;
	} else {
	    /*
	     * Note, that we don't lock the auth file here, as it's
	     * temporary - we can assume, that we are the only ones
	     * knowing about this file anyway.
	     */
#ifdef HAS_MKSTEMP
	    sprintf (name_buf, "%s/.XauthXXXXXX", d->userAuthDir);
	    new = fdOpenW (mkstemp (name_buf));
#else
	    for (i = 0; i < 100; i++) {
		sprintf (name_buf, "%s/.XauthXXXXXX", d->userAuthDir);
		(void) mktemp (name_buf);
		if ((new = 
		     fdOpenW (open (name_buf, O_WRONLY | O_CREAT | O_EXCL, 
				    0600))))
		    break;
	    }
#endif
	    if (!new) {
		LogError ("can't create authorization file in %s\n", 
			  d->userAuthDir);
		return;
	    }
	    name = 0;
	    envname = name_buf;
	    old = 0;
	}
	doWrite = 1;
	Debug ("%d authorization protocols for %s\n", d->authNum, d->name);
	/*
	 * Write MIT-MAGIC-COOKIE-1 authorization first, so that
	 * R4 clients which only knew that, and used the first
	 * matching entry will continue to function
	 */
	magicCookie = -1;
	for (i = 0; i < d->authNum; i++)
	{
	    if (auths[i]->name_length == 18 &&
		!memcmp (auths[i]->name, "MIT-MAGIC-COOKIE-1", 18))
	    {
		magicCookie = i;
	    	if ((d->displayType & d_location) == dLocal)
	    	    writeLocalAuth (new, auths[i], d->name);
#ifdef XDMCP
	    	else
	    	    writeRemoteAuth (new, auths[i], (XdmcpNetaddr)d->peer.data, 
				     d->peer.length, d->name);
#endif
		break;
	    }
	}
	/* now write other authorizations */
	for (i = 0; i < d->authNum; i++)
	{
	    if (i != magicCookie)
	    {
		data_len = auths[i]->data_length;
		/* client will just use default Kerberos cache, so don't
		 * even write cache info into the authority file.
		 */
		if (auths[i]->name_length == 14 &&
		    !strncmp (auths[i]->name, "MIT-KERBEROS-5", 14))
		    auths[i]->data_length = 0;
	    	if ((d->displayType & d_location) == dLocal)
	    	    writeLocalAuth (new, auths[i], d->name);
#ifdef XDMCP
	    	else
	    	    writeRemoteAuth (new, auths[i], (XdmcpNetaddr)d->peer.data, 
				     d->peer.length, d->name);
#endif
		auths[i]->data_length = data_len;
	    }
	}
	endUserAuth (old, new, new_name);
	if (name)
	    envname = moveUserAuth (name, new_name, envname);
	if (envname) {
	    verify->userEnviron = setEnv (verify->userEnviron,
					  "XAUTHORITY", envname);
	    verify->systemEnviron = setEnv (verify->systemEnviron,
					    "XAUTHORITY", envname);
	}
	/* a chown() used to be here, but this code runs as user anyway */
    }
    Debug ("done SetUserAuthorization\n");
}

void
RemoveUserAuthorization (struct display *d, struct verify_info *verify)
{
    Xauth   **auths;
    FILE    *old, *new;
    int	    i;
    char    name[1024], new_name[1024];

    if (!(auths = d->authorizations))
	return;
    Debug ("RemoveUserAuthorization\n");
    startUserAuth (verify, name, new_name, &old, &new);
    if (new)
    {
	doWrite = 0;
	for (i = 0; i < d->authNum; i++)
	{
	    if ((d->displayType & d_location) == dLocal)
	    	writeLocalAuth (new, auths[i], d->name);
#ifdef XDMCP
	    else
	    	writeRemoteAuth (new, auths[i], (XdmcpNetaddr)d->peer.data, 
				 d->peer.length, d->name);
#endif
	}
	doWrite = 1;
	endUserAuth (old, new, new_name);
	(void) moveUserAuth (name, new_name, 0);
    }
    Debug ("done RemoveUserAuthorization\n");
}
