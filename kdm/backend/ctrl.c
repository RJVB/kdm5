/*

Copyright 1988, 1998  The Open Group
Copyright 2001-2004 Oswald Buddenhagen <ossi@kde.org>

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
IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the copyright holder.

*/

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * display manager
 */

#include "dm.h"
#include "dm_socket.h"
#include "dm_error.h"

#include <string.h>


static void
acceptSock (CtrlRec *cr)
{
    struct cmdsock *cs;
    int fd;
    
    if ((fd = accept(cr->fd, 0, 0)) < 0) {
      bust:
	LogError ("Error accepting command connection\n");
	return;
    }
    if (!(cs = Malloc (sizeof(*cs)))) {
	close (fd);
	goto bust;
    }
    cs->sock.fd = fd;
    cs->sock.buffer = 0;
    cs->sock.buflen = 0;
    cs->next = cr->css;
    cr->css = cs;
    fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) | O_NONBLOCK);
    RegisterCloseOnFork (fd);
    RegisterInput (fd);
}

static void
nukeSock (struct cmdsock *cs)
{
    UnregisterInput (cs->sock.fd);
    CloseNClearCloseOnFork (cs->sock.fd);
    if (cs->sock.buffer)
	free (cs->sock.buffer);
    free (cs);
}


static CtrlRec ctrl = { 0, 0, -1, 0, 0, { -1, 0, 0 } };

void
openCtrl (struct display *d)
{
    CtrlRec *cr;
    const char *dname;
    char *sockdir;
    struct sockaddr_un sa;

    if (!*fifoDir)
	return;
    if (d)
	cr = &d->ctrl, dname = d->name;
    else
	cr = &ctrl, dname = 0;
    if (cr->fifo.fd < 0) {
	if (mkdir (fifoDir, 0755)) {
	    if (errno != EEXIST) {
		LogError ("mkdir %\"s failed; no control FiFos will be available\n", 
			  fifoDir);
		return;
	    }
	} else
	    chmod (fifoDir, 0755); /* override umask */
	StrApp (&cr->fpath, fifoDir, dname ? "/xdmctl-" : "/xdmctl", 
		dname, (char *)0);
	if (cr->fpath) {
	    unlink (cr->fpath);
	    if (mkfifo (cr->fpath, 0) < 0)
		LogError ("Cannot create control FiFo %\"s\n", cr->fpath);
	    else {
		cr->gid = fifoGroup;
		chown (cr->fpath, -1, fifoGroup);
		chmod (cr->fpath, 0620);
		if ((cr->fifo.fd = open (cr->fpath, O_RDWR | O_NONBLOCK)) >= 0) {
		    RegisterCloseOnFork (cr->fifo.fd);
		    RegisterInput (cr->fifo.fd);
		    goto fifok;
		}
		unlink (cr->fpath);
		LogError ("Cannot open control FiFo %\"s\n", cr->fpath);
	    }
	    free (cr->fpath);
	    cr->fpath = 0;
	}
    }
  fifok:
    if (cr->fd < 0) {
	/* fifoDir is created above already */
	sockdir = 0;
	StrApp (&sockdir, fifoDir, dname ? "/dmctl-" : "/dmctl", 
		dname, (char *)0);
	if (sockdir) {
	    StrApp (&cr->path, sockdir, "/socket", (char *)0);
	    if (cr->path) {
		if (strlen (cr->path) >= sizeof(sa.sun_path))
		    LogError ("path %\"s too long; no control sockets will be available\n", 
			      cr->path);
		else if (mkdir (sockdir, 0755) && errno != EEXIST)
		    LogError ("mkdir %\"s failed; no control sockets will be available\n", 
			      sockdir);
		else {
		    chown (sockdir, -1, fifoGroup);
		    chmod (sockdir, 0750);
		    if ((cr->fd = socket (PF_UNIX, SOCK_STREAM, 0)) < 0)
			LogError ("Cannot create control socket\n");
		    else {
			unlink (cr->path);
			sa.sun_family = AF_UNIX;
			strcpy (sa.sun_path, cr->path);
			if (!bind (cr->fd, (struct sockaddr *)&sa, sizeof(sa))) {
			    if (!listen (cr->fd, 5)) {
				chmod (cr->path, 0666);
				RegisterCloseOnFork (cr->fd);
				RegisterInput (cr->fd);
				free (sockdir);
				return;
			    }
			    unlink (cr->path);
			    LogError ("Cannot listen on control socket %\"s\n",
				      cr->path);
			} else
			    LogError ("Cannot bind control socket %\"s\n",
				      cr->path);
			close (cr->fd);
			cr->fd = -1;
		    }
		}
		free (cr->path);
		cr->path = 0;
	    }
	    free (sockdir);
	}
    }
}

void
closeCtrl (struct display *d)
{
    CtrlRec *cr = d ? &d->ctrl : &ctrl;

    if (cr->fd >= 0) {
	UnregisterInput (cr->fd);
	CloseNClearCloseOnFork (cr->fd);
	cr->fd = -1;
	unlink (cr->path);
	*strrchr (cr->path, '/') = 0;
	rmdir (cr->path);
	*strrchr (cr->path, '/') = 0;
	rmdir (cr->path);
	free (cr->path);
	cr->path = 0;
	while (cr->css) {
	    struct cmdsock *cs = cr->css;
	    cr->css = cs->next;
	    nukeSock (cs);
	}
    }
    if (cr->fifo.fd >= 0) {
	UnregisterInput (cr->fifo.fd);
	CloseNClearCloseOnFork (cr->fifo.fd);
	cr->fifo.fd = -1;
	unlink (cr->fpath);
	*strrchr (cr->fpath, '/') = 0;
	rmdir (cr->fpath);
	free (cr->fpath);
	cr->fpath = 0;
	if (cr->fifo.buffer)
	    free (cr->fifo.buffer);
	cr->fifo.buffer = 0;
	cr->fifo.buflen = 0;
    }
}

void
chownCtrl (CtrlRec *cr, int uid, int gid)
{
    if (cr->fpath)
	chown (cr->fpath, uid, gid);
    if (cr->path) {
	char *ptr = strrchr (cr->path, '/');
	*ptr = 0;
	chown (cr->path, uid, gid);
	*ptr = '/';
    }
}

void
updateCtrl (void)
{
    char *ffp;
    unsigned ffl;

    if (ctrl.path) {
	ffp = ctrl.path;
	ffl = strrchr (ffp, '/') - ffp;
    } else
	ffl = 0;
    if (ffl != strlen (fifoDir) || memcmp (fifoDir, ffp, ffl)) {
	closeCtrl (0);
	openCtrl (0);
    } else if (ctrl.gid != fifoGroup) {
	ctrl.gid = fifoGroup;
	chownCtrl (&ctrl, -1, fifoGroup);
    }
}


static void
fLog (struct display *d, int fd, const char *sts, const char *msg, ...)
{
    char *fmsg, *otxt;
    const char *what;
    int olen;
    va_list va;

    va_start (va, msg);
    VASPrintf (&fmsg, msg, va);
    va_end (va);
    if (!fmsg)
	return;
    if (fd >= 0) {
	olen = ASPrintf (&otxt, "%s\t%\\s\n", sts, fmsg);
	if (otxt) {
	    Writer (fd, otxt, olen);
	    free (otxt);
	}
	what = "socket";
    } else
	what = "FiFo";
    if (d)
	Debug ("control %s for %s: %s - %s", what, d->name, sts, fmsg);
    else
	Debug ("global control %s: %s - %s", what, sts, fmsg);
    free (fmsg);
}


static char *
unQuote (const char *str)
{
    char *ret, *adp;

    if (!(ret = Malloc (strlen (str) + 1)))
	return 0;
    for (adp = ret; *str; str++, adp++)
	if (*str == '\\')
	    switch (*++str) {
	    case 0: str--; /* fallthrough */
	    case '\\': *adp = '\\'; break;
	    case 'n': *adp = '\n'; break;
	    case 't': *adp = '\t'; break;
	    default: *adp++ = '\\'; *adp = *str; break;
	    }
	else
	    *adp = *str;
    *adp = 0;
    return ret;
}

static void
processCtrl (const char *string, int len, int fd, struct display *d)
{
    const char *word;
    char **ar, *args;
    int how, when;
    char cbuf[32];

    if (!(ar = initStrArr (0)))
	return;
    for (word = string; ; string++, len--)
	if (!len || *string == '\t') {
	    if (!(ar = addStrArr (ar, word, string - word)))
		return;
	    if (!len)
		break;
	    word = string + 1;
	}
    word = fd >= 0 ? "socket" : "FiFo";
    if (d)
	Debug ("control %s for %s received %'[s\n", word, d->name, ar);
    else
	Debug ("global control %s received %'[s\n", word, ar);
    if (ar[0]) {
	if (fd >= 0 && !strcmp (ar[0], "caps")) {
	    Writer (fd, "ok\tkdm\t", 7);
	    if (d) {
		if (d->allowShutdown != SHUT_NONE) {
		    if (d->allowShutdown == SHUT_ROOT && d->userSess)
			Writer (fd, "shutdown root\t", 14);
		    else
			Writer (fd, "shutdown\t", 9);
		    if (d->allowNuke != SHUT_NONE) {
			if (d->allowNuke == SHUT_ROOT && d->userSess)
			    Writer (fd, "nuke root\t", 10);
			else
			    Writer (fd, "nuke\t", 5);
		    }
		}
		if ((d->displayType & d_location) == dLocal &&
		    AnyReserveDisplays ())
		    Writer (fd, cbuf, sprintf (cbuf, "reserve %d\t",
					       idleReserveDisplays ()));
		Writer (fd, "lock\tsuicide\n", 21);
	    } else {
		if (fifoAllowShutdown) {
		    Writer (fd, "shutdown\t", 9);
		    if (fifoAllowNuke)
			Writer (fd, "nuke\t", 5);
		}
		Writer (fd, "login\n", 6);
	    }
	    goto bust;
	} else if (fd >= 0 && !strcmp (ar[0], "list")) {
	} else if (!strcmp (ar[0], "shutdown")) {
	    if (!ar[1] || (!ar[2] && !d)) {
		fLog (d, fd, "bad", "missing argument(s)");
		goto bust;
	    }
	    if (!strcmp (ar[1], "reboot"))
		how = SHUT_REBOOT;
	    else if (!strcmp (ar[1], "halt"))
		how = SHUT_HALT;
	    else {
		fLog (d, fd, "bad", "invalid type %\"s", ar[1]);
		goto bust;
	    }
	    if (ar[2]) {
		if (!strcmp (ar[2], "forcenow"))
		    when = SHUT_FORCENOW;
		else if (!strcmp (ar[2], "trynow"))
		    when = SHUT_TRYNOW;
		else if (!strcmp (ar[2], "schedule"))
		    when = SHUT_SCHEDULE;
		else {
		    fLog (d, fd, "bad", "invalid mode %\"s", ar[2]);
		    goto bust;
		}
	    } else
		when = d ? d->defSdMode : -1;
	    if (d) {
		if (d->allowShutdown == SHUT_NONE ||
		    (d->allowShutdown == SHUT_ROOT && d->userSess))
		{
		    fLog (d, fd, "perm", "shutdown forbidden");
		    goto bust;
		}
		if (when == SHUT_FORCENOW &&
		    (d->allowNuke == SHUT_NONE ||
		    (d->allowNuke == SHUT_ROOT && d->userSess)))
		{
		    fLog (d, fd, "perm", "forced shutdown forbidden");
		    goto bust;
		}
		d->hstent->sd_how = how;
		d->hstent->sd_when = when;
	    } else {
		if (!fifoAllowShutdown) {
		    fLog (d, fd, "perm", "shutdown forbidden");
		    goto bust;
		}
		if (when == SHUT_FORCENOW && !fifoAllowNuke) {
		    fLog (d, fd, "perm", "forced shutdown forbidden");
		    goto bust;
		}
		doShutdown (how, when);
	    }
	} else if (d) {
	    if (!strcmp (ar[0], "lock")) {
		d->hstent->lock = 1;
#ifdef AUTO_RESERVE
		if (AllLocalDisplaysLocked (0))
		    StartReserveDisplay (0);
#endif
	    } else if (!strcmp (ar[0], "unlock")) {
		d->hstent->lock = 0;
#ifdef AUTO_RESERVE
		ReapReserveDisplays ();
#endif
	    } else if (!strcmp (ar[0], "reserve")) {
		if ((d->displayType & d_location) == dLocal) {
		    int lt = 0;
		    if (ar[1])
			lt = atoi (ar[1]);
		    /* XXX make timeout configurable? */
		    if (!StartReserveDisplay (lt > 15 ? lt : 60)) {
			fLog (d, fd, "noent", "no reserve display available");
			goto bust;
		    }
		} else {
		    fLog (d, fd, "perm", "display is not local");
		    goto bust;
		}
	    } else if (!strcmp (ar[0], "suicide")) {
		if (d->status == running && d->pid != -1) {
		    TerminateProcess (d->pid, SIGTERM);
		    d->status = raiser;
		}
	    } else {
		fLog (d, fd, "nosys", "unknown command");
		goto bust;
	    }
	} else {
	    if (!strcmp (ar[0], "login")) {
		if (arrLen (ar) < 5) {
		    fLog (d, fd, "bad", "missing argument(s)");
		    goto bust;
		}
		if (!(d = FindDisplayByName (ar[1]))) {
		    fLog (d, fd, "noent", "display %s not found", ar[1]);
		    goto bust;
		}
		if (ar[5] && (args = unQuote (ar[5]))) {
		    setNLogin (d, ar[3], ar[4], args, 2);
		    free (args);
		} else
		    setNLogin (d, ar[3], ar[4], 0, 2);
		if (d->status == running && d->pid != -1) {
		    if (d->userSess < 0 || !strcmp (ar[2], "now")) {
			TerminateProcess (d->pid, SIGTERM);
			d->status = raiser;
		    }
		} else if (d->status == reserve)
		    d->status = notRunning;
		else if (d->status == textMode && !strcmp (ar[2], "now"))
		    SwitchToX (d);
	    } else {
		fLog (d, fd, "nosys", "unknown command");
		goto bust;
	    }
	}
	if (fd >= 0)
	    Writer (fd, "ok\n", 3);
    }
  bust:
    freeStrArr (ar);
}

static int
handleChan (struct display *d, struct bsock *cs, int fd, FD_TYPE *reads)
{
    char *bufp, *nbuf, *obuf, *eol;
    int len, bl, llen;
    char buf[1024];

    bl = cs->buflen;
    obuf = cs->buffer;
    if (bl <= 0 && FD_ISSET (cs->fd, reads)) {
	FD_CLR (cs->fd, reads);
	bl = -bl;
	memcpy (buf, obuf, bl);
	if ((len = Reader (cs->fd, buf + bl, sizeof(buf) - bl)) <= 0)
	    return -1;
	bl += len;
	bufp = buf;
    } else {
	len = 0;
	bufp = obuf;
    }
    if (bl > 0) {
	if ((eol = memchr (bufp, '\n', bl))) {
	    llen = eol - bufp + 1;
	    bl -= llen;
	    if (bl) {
		if (!(nbuf = Malloc (bl))) 
		    return -1;
		memcpy (nbuf, bufp + llen, bl);
	    } else
		nbuf = 0;
	    cs->buffer = nbuf;
	    cs->buflen = bl;
	    processCtrl (bufp, llen - 1, fd, d);
	    if (obuf)
		free (obuf);
	    return 1;
	} else if (!len) {
	    if (fd >= 0)
		cs->buflen = -bl;
	    else
		fLog (d, -1, "bad", "unterminated command");
	}
    }
    return 0;
}

int
handleCtrl (FD_TYPE *reads, struct display *d)
{
    CtrlRec *cr = d ? &d->ctrl : &ctrl;
    struct cmdsock *cs, **csp;

    if (cr->fifo.fd >= 0) {
	switch (handleChan (d, &cr->fifo, -1, reads)) {
	case -1:
	    if (cr->fifo.buffer)
		free (cr->fifo.buffer);
	    cr->fifo.buflen = 0;
	    break;
	case 1:
	    return 1;
	default:
	    break;
	}
    }
    if (cr->fd >= 0 && FD_ISSET (cr->fd, reads))
	acceptSock (cr);
    else {
	for (csp = &cr->css; (cs = *csp); ) {
	    switch (handleChan (d, &cs->sock, cs->sock.fd, reads)) {
	    case -1:
		*csp = cs->next;
		nukeSock (cs);
		continue;
	    case 1:
		return 1;
	    default:
		break;
	    }
	    csp = &cs->next;
	}
    }
    return 0;
}
