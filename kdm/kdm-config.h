    /*

    $Id$

    Copyright (C) 1997, 1998 Steffen Hansen <hansen@kde.org>
    Copyright (C) 2000 Oswald Buddenhagen <ossi@kde.org>


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
 
#ifndef KDM_CONFIG_H_
#define KDM_CONFIG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* to avoid INT32 conflict between <qglobal.h> and <X/Xmd.h>. */ 
#ifndef QT_CLEAN_NAMESPACE
# define QT_CLEAN_NAMESPACE
#endif

/* xdm stuff */
#define UNIXCONN
#define TCPCONN
#undef GREET_USER_STATIC

#if defined(GREETER) && !defined(GREET_USER_STATIC)
# define GREET_LIB
#endif

#ifdef XDMBINDIR
# define BINDIR XDMBINDIR
#endif

#define DEF_XDM_CONFIG XDMDIR##"/xdm-config"
#define DEF_AUTH_DIR XDMDIR##"/authdir"

/* If this isn't defined, we crash boxes with S3 cards.  
 * See genauth.c  
 */ 
#define FRAGILE_DEV_MEM

/* Authorization stuff */
/*
 * How do we check for HASXDMAUTH? Use Imake ?? 
*/
/*
#if defined(HAVE_KRB5_KRB5_H)
# define K5AUTH
#endif
*/
/* Too many systems have trouble with secure rpc,
   so its disabled for now:
#if defined(HAVE_RPC_RPC_H) && defined(HAVE_RPC_KEY_PROT_H)
# define SECURE_RPC
#endif
*/

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifdef HAVE_PAM
# define USE_PAM
#else
# ifdef HAVE_SHADOW
#  define USESHADOW
# endif
#endif

#ifdef HAVE_SYSLOG_H
# define USE_SYSLOG
#endif

#ifndef _PATH_VARRUN
#define _PATH_VARRUN "/var/run/"
#endif

#ifndef _PATH_MEM
#define _PATH_MEM "/dev/mem"
#endif

#ifdef sun
#define SVR4
#endif

#ifdef SVR4
#define NeedVarargsPrototypes
#endif

/*
 * These values define what is called by KDM on Shutdown or Reboot
 * respectively. Default is /sbin/halt and /sbin/reboot
 */
#if defined(__NetBSD__) || defined(__FreeBSD__)
#define SHUTDOWN_CMD	"/sbin/shutdown -h now"
#define REBOOT_CMD	"/sbin/shutdown -r now"
#endif

#ifdef __SVR4
#define SHUTDOWN_CMD	"/usr/sbin/halt"
#define REBOOT_CMD	"/usr/sbin/reboot"
#endif

#ifndef SHUTDOWN_CMD
#define	SHUTDOWN_CMD	"/sbin/halt"
#endif
#ifndef REBOOT_CMD
#define REBOOT_CMD	"/sbin/reboot"
#endif

#ifdef HAVE_SETUSERCONTEXT
#define HAS_SETUSERCONTEXT
#endif

#endif /* KDM_CONFIG_H */
