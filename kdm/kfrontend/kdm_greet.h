    /*

    KDE Greeter module for xdm
    $Id$

    Copyright (C) 2001 Oswald Buddenhagen <ossi@kde.org>

    This file contains code from the old xdm core,
    Copyright 1988, 1998  Keith Packard, MIT X Consortium/The Open Group

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    */

#ifndef _KDM_GREET_H_
#define _KDM_GREET_H_

#include "kdm_config.h"

#include <X11/Xlib.h>

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void GSendInt (int val);
void GSendStr (const char *buf);
int GRecvInt ();
char *GRecvStr ();
char **GRecvStrArr (int *len);
int GetCfgInt (int id);
char *GetCfgStr (int id);
char **GetCfgStrArr (int id, int *len);

void SessionExit (int ret) ATTR_NORETURN;

void Debug (char *fmt, ...) ATTR_PRINTFLIKE(1,2);
void LogInfo (char *fmt, ...) ATTR_PRINTFLIKE(1,2);
void LogError (char *fmt, ...) ATTR_PRINTFLIKE(1,2);
void LogPanic (char *fmt, ...) ATTR_PRINTFLIKE(1,2) ATTR_NORETURN;

void SecureDisplay (Display *dpy);
void UnsecureDisplay (Display *dpy);

extern char *dname;		/* d->name */
extern int disLocal;		/* d->displayType.location == Local */
extern int dhasConsole;		/* !isEmpty(d->console) */
extern int dgrabServer;		/* d->grabServer */

#ifdef __cplusplus
}
#endif

#endif /* _KDM_GREET_H_ */
