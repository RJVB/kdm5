/* $TOG: verify.c /main/37 1998/02/11 10:00:45 kaleb $ */
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
/* $XFree86: xc/programs/xdm/greeter/verify.c,v 3.9 2000/06/14 00:16:16 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * verify.c
 *
 * user verification and session initiation.
 */

#include "dm.h"
#include "dm_auth.h"
#include "dm_error.h"

#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#ifdef SECURE_RPC
# include <rpc/rpc.h>
# include <rpc/key_prot.h>
#endif
#ifdef K5AUTH
# include <krb5/krb5.h>
#endif
#ifdef CSRG_BASED
# ifdef HAS_SETUSERCONTEXT
#  include <login_cap.h>
#  define USE_LOGIN_CAP 1
# endif
#endif
#ifdef USE_PAM
# ifdef __DARWIN__
#  include <pam/pam_appl.h>
# else
#  include <security/pam_appl.h>
# endif
#elif defined(AIXV3) /* USE_PAM */
# include <login.h>
# include <usersec.h>
extern int loginrestrictions (const char *Name, const int Mode, const char *Tty, char **Msg);
extern int loginfailed (const char *User, const char *Host, const char *Tty);
extern int loginsuccess (const char *User, const char *Host, const char *Tty, char **Msg);
#else /* USE_PAM || AIXV3 */
# ifdef USESHADOW
#  include <shadow.h>
# endif
# ifdef KERBEROS
#  include <sys/param.h>
#  include <krb.h>
#  ifndef NO_AFS
#   include <kafs.h>
#  endif
# endif
/* for nologin */
# include <sys/types.h>
# include <unistd.h>
/* for expiration */
# include <time.h>
#endif	/* USE_PAM || AIXV3 */

#if defined(__osf__) || defined(linux) || defined(__QNXNTO__) || defined(__GNU__)
# define setpgrp setpgid
#endif

#ifdef QNX4
extern char *crypt(const char *, const char *);
#endif

/*
 * Session data, mostly what struct verify_info was for
 */
char *curuser;
char *curpass;
char *curdmrc;
char *newdmrc;
char **userEnviron;
char **systemEnviron;
static int curuid;
static int curgid;

static struct passwd *p;
#ifdef USE_PAM
static pam_handle_t *pamh;
#elif defined(AIXV3)
static char tty[16], hostname[100];
#else
# ifdef USESHADOW
static struct spwd *sp;
# endif
# ifdef KERBEROS
static char krbtkfile[MAXPATHLEN];
# endif
#endif

#ifdef USE_PAM

static char *infostr, *errstr;

# ifdef sun
typedef struct pam_message pam_message_type;
# else
typedef const struct pam_message pam_message_type;
# endif

static int
PAM_conv (int num_msg,
	  pam_message_type **msg,
	  struct pam_response **resp,
	  void *appdata_ptr ATTR_UNUSED)
{
    int count;
    struct pam_response *reply;

    if (!(reply = calloc(num_msg, sizeof(*reply))))
	return PAM_CONV_ERR;

    for (count = 0; count < num_msg; count++) {
	switch (msg[count]->msg_style) {
	case PAM_TEXT_INFO:
	    if (!StrApp(&infostr, msg[count]->msg, "\n", (char *)0))
		goto conv_err;
	    break;
	case PAM_ERROR_MSG:
	    if (!StrApp(&errstr, msg[count]->msg, "\n", (char *)0))
		goto conv_err;
	    break;
	case PAM_PROMPT_ECHO_OFF:
	    /* wants password */
# ifndef PAM_FAIL_DELAY
	    if (!curpass[0])
		goto conv_err;
# endif
	    if (!StrDup (&reply[count].resp, curpass))
		goto conv_err;
	    reply[count].resp_retcode = PAM_SUCCESS;
	    break;
	case PAM_PROMPT_ECHO_ON:
	    /* user name given to PAM already */
	    /* fall through */
	default:
	    /* unknown */
	    goto conv_err;
	}
    }
    *resp = reply;
    return PAM_SUCCESS;

  conv_err:
    for (; count >= 0; count--)
	if (reply[count].resp) {
	    switch (msg[count]->msg_style) {
	    case PAM_ERROR_MSG:
	    case PAM_TEXT_INFO:
	    case PAM_PROMPT_ECHO_ON:
		free(reply[count].resp);
		break;
	    case PAM_PROMPT_ECHO_OFF:
		WipeStr(reply[count].resp);
		break;
	    }
	    reply[count].resp = 0;
	}
    /* forget reply too */
    free (reply);
    return PAM_CONV_ERR;
}

static struct pam_conv PAM_conversation = {
	PAM_conv,
	NULL
};

# ifdef PAM_FAIL_DELAY
static void
fail_delay(int retval ATTR_UNUSED, unsigned usec_delay ATTR_UNUSED, 
	   void *appdata_ptr ATTR_UNUSED)
{}
# endif

#endif /* USE_PAM */

static int
AccNoPass (struct display *d, const char *un)
{
    char **fp;

    if (!strcmp (un, d->autoUser))
	return 1;

    for (fp = d->noPassUsers; *fp; fp++)
	if (!strcmp (un, *fp))
	    return 1;

    return 0;
}

int
Verify (struct display *d, const char *name, const char *pass)
{
#if !defined(USE_PAM) && defined(AIXV3)
    int		i, reenter;
    char	*msg;
#endif

    Debug ("Verify %s ...\n", name);

    if (!strlen (name)) {
	Debug ("empty user name provided.\n");
	return V_AUTH;
    }

    if (ReStr (&curuser, name) == 2 && curdmrc) {
	free (curdmrc);
	curdmrc = 0;
    }
    ReStr (&curpass, pass);

#ifdef USE_PAM

    if (!pamh) {
	int pretc;
	Debug("opening new PAM handle\n");
	if (pam_start(PAMService, curuser, &PAM_conversation, &pamh) != PAM_SUCCESS) {
	    ReInitErrorLog ();
	    return V_ERROR;
	}
	if ((pretc = pam_set_item(pamh, PAM_TTY, d->name)) != PAM_SUCCESS) {
	    pam_end(pamh, pretc);
	    pamh = NULL;
	    ReInitErrorLog ();
	    return V_ERROR;
	}
	if ((pretc = pam_set_item(pamh, PAM_RHOST, "")) != PAM_SUCCESS) {
	    pam_end(pamh, pretc);
	    pamh = NULL;
	    ReInitErrorLog ();
	    return V_ERROR;
	}
# ifdef PAM_FAIL_DELAY
	pam_set_item(pamh, PAM_FAIL_DELAY, (void *)fail_delay);
# endif
    } else
	if (pam_set_item(pamh, PAM_USER, curuser) != PAM_SUCCESS) {
	    ReInitErrorLog ();
	    return V_ERROR;
	}
    ReInitErrorLog ();


    if (infostr) {
	free (infostr);
	infostr = 0;
    }

    if (errstr) {
	free (errstr);
	errstr = 0;
    }

#elif defined(AIXV3)

    if ((d->displayType & d_location) == dForeign) {
	char *tmpch;
	strncpy(hostname, d->name, sizeof(hostname) - 1);
	hostname[sizeof(hostname)-1] = '\0';
	if ((tmpch = strchr(hostname, ':')))
	    *tmpch = '\0';
    } else
	hostname[0] = '\0';

    /* tty names should only be 15 characters long */
# if 0
    for (i = 0; i < 15 && d->name[i]; i++) {
	if (d->name[i] == ':' || d->name[i] == '.')
	    tty[i] = '_';
	else
	    tty[i] = d->name[i];
    }
    tty[i] = '\0';
# else
    memcpy(tty, "/dev/xdm/", 9);
    for (i = 0; i < 6 && d->name[i]; i++) {
	if (d->name[i] == ':' || d->name[i] == '.')
	    tty[9 + i] = '_';
	else
	    tty[9 + i] = d->name[i];
    }
    tty[9 + i] = '\0';
# endif

#endif

#if !defined(USE_PAM) && !defined(AIXV3)

    if (!(p = getpwnam (name))) {
	Debug ("getpwnam() failed.\n");
	return V_AUTH;
    }
# ifdef linux	/* only Linux? */
    if (p->pw_passwd[0] == '!' || p->pw_passwd[0] == '*') {
	Debug ("account is locked\n");
	return V_AUTH;
    }
# endif

# ifdef USESHADOW
    if ((sp = getspnam(name)))
	p->pw_passwd = sp->sp_pwdp;
    else
	Debug ("getspnam() failed: %s.  Are you root?\n", SysErrorMsg());
# endif

#endif /* !defined(USE_PAM) && !defined(AIXV3) */

    if (!curpass[0] && AccNoPass (d, curuser)) {
	Debug ("accepting despite empty password\n");
	return V_OK;
    }

#if !defined(USE_PAM) && !defined(AIXV3)

# ifdef KERBEROS
    if (p->pw_uid)
    {
	int ret;
	char realm[REALM_SZ];

	if (krb_get_lrealm(realm, 1)) {
	    LogError("Can't get KerberosIV realm.\n");
	    return V_ERROR;
	}

	sprintf(krbtkfile, "%s.%.*s", TKT_ROOT, MAXPATHLEN - strlen(TKT_ROOT) - 2, d->name);
	krb_set_tkt_string(krbtkfile);
	unlink(krbtkfile);

	ret = krb_verify_user(curuser, "", realm, curpass, 1, "rcmd");
	if (ret == KSUCCESS) {
	    chown(krbtkfile, p->pw_uid, p->pw_gid);
	    Debug("KerberosIV verify succeeded\n");
	    goto done;
	} else if (ret != KDC_PR_UNKNOWN && ret != SKDC_CANT) {
	    LogError("KerberosIV verification failure %\"s for %s\n",
		     krb_get_err_text(ret), curuser);
	    krbtkfile[0] = '\0';
	    return V_ERROR;
	}
	Debug("KerberosIV verify failed: %s\n", krb_get_err_text(ret));
    }
    krbtkfile[0] = '\0';
# endif  /* KERBEROS */

# if defined(ultrix) || defined(__ultrix__)
    if (authenticate_user(p, curpass, NULL) < 0)
# else
    if (strcmp (crypt (curpass, p->pw_passwd), p->pw_passwd))
# endif
	if (!d->allowNullPasswd || p->pw_passwd[0]) {
	    Debug ("password verify failed\n");
	    return V_AUTH;
	} /* else: null passwd okay */

# ifdef KERBEROS
  done:
# endif

#endif /* !defined(USE_PAM) && !defined(AIXV3) */

#ifdef USE_PAM

    if (pam_authenticate(pamh, d->allowNullPasswd ?
				0 : PAM_DISALLOW_NULL_AUTHTOK) != PAM_SUCCESS) {
	ReInitErrorLog ();
	return V_AUTH;
    }
    ReInitErrorLog ();

#elif defined(AIXV3) /* USE_PAM */

    enduserdb();
    msg = NULL;
    if (authenticate(curuser, curpass, &reenter, &msg) || reenter) {
	Debug("authenticate() - %s\n", msg ? msg : "error");
	if (msg)
	    free((void *)msg);
	loginfailed(curuser, hostname, tty);
	return V_AUTH;
    }
    if (msg)
	free((void *)msg);

#endif /* USE_PAM && AIXV3 */

    Debug ("verify succeeded\n");

    return V_OK;
}

void
Restrict (struct display *d)
{
#ifdef USE_PAM
    int			pretc;
#else
# ifdef AIXV3
    char		*msg;
# else /* AIXV3 */
    struct stat		st;
    const char		*nolg;
#  ifdef HAVE_GETUSERSHELL
    char		*s;
#  endif
#  if defined(HAVE_PW_EXPIRE) || defined(USESHADOW)
    int			tim, expir, warntime;
    int			quietlog;
    int			expire, retv;
#  endif
#  ifdef USE_LOGIN_CAP
#   ifdef HAVE_LOGIN_GETCLASS
    login_cap_t		*lc;
#   else
    struct login_cap	*lc;
#   endif
#  endif
# endif /* AIXV3 */
#endif

    Debug("Restrict %s ...\n", curuser);

#if defined(USE_PAM) || defined(AIXV3)
    if (!(p = getpwnam (curuser))) {
	LogError ("getpwnam(%s) failed.\n", curuser);
	GSendInt (V_ERROR);
	return;
    }
#endif
    if (!p->pw_uid) {
	if (!d->allowRootLogin)
	    GSendInt (V_NOROOT);
	else
	    GSendInt (V_OK);	/* don't deny root to log in */
	return;
    }

#ifdef USE_PAM

    pretc = pam_acct_mgmt(pamh, 0);
    ReInitErrorLog ();
    if (errstr) {
	GSendInt (V_MSGERR);
	GSendStr (errstr);
    } else if (pretc != PAM_SUCCESS) {
	GSendInt (V_AUTH);
    } else if (infostr) {
	GSendInt (V_MSGINFO);
	GSendStr (infostr);
    } else
	GSendInt (V_OK);
    /* really should do password changing, but it doesn't fit well */

#elif defined(AIXV3)	/* USE_PAM */

    msg = NULL;
    if (loginrestrictions(curuser,
	((d->displayType & d_location) == dForeign) ? S_RLOGIN : S_LOGIN,
	tty, &msg) == -1)
    {
	Debug("loginrestrictions() - %s\n", msg ? msg : "Error\n");
	loginfailed(curuser, hostname, tty);
	if (msg) {
	    GSendInt (V_MSGERR);
	    GSendStr (msg);
	} else
	    GSendInt (V_AUTH);
    } else
	    GSendInt (V_OK);
    if (msg)
	free((void *)msg);

#else	/* USE_PAM || AIXV3 */

# ifdef HAVE_GETUSERSHELL
    for (;;) {
	if (!(s = getusershell())) {
	    Debug("shell not in /etc/shells\n");
	    endusershell();
	    GSendInt (V_BADSHELL);
	    return;
	}
	if (!strcmp(s, p->pw_shell)) {
	    endusershell();
	    break;
	}
    }
# endif

# ifdef USE_LOGIN_CAP
#  ifdef HAVE_LOGIN_GETCLASS
    lc = login_getclass(p->pw_class);
#  else
    lc = login_getpwclass(p);
#  endif
    if (!lc) {
	GSendInt (V_ERROR);
	return;
    }
# endif


/* restrict_nologin */
# ifndef _PATH_NOLOGIN
#  define _PATH_NOLOGIN "/etc/nologin"
# endif

    if ((
# ifdef USE_LOGIN_CAP
    /* Do we ignore a nologin file? */
	!login_getcapbool(lc, "ignorenologin", 0)) &&
	(!stat((nolg = login_getcapstr(lc, "nologin", "", NULL)), &st) ||
# endif
	 !stat((nolg = _PATH_NOLOGIN), &st))) {
	GSendInt (V_NOLOGIN);
	GSendStr (nolg);
# ifdef USE_LOGIN_CAP
	login_close(lc);
# endif
	return;
    }


/* restrict_nohome */
# ifdef USE_LOGIN_CAP
    if (login_getcapbool(lc, "requirehome", 0)) {
	struct stat st;
	if (!*p->pw_dir || stat (p->pw_dir, &st) || st.st_uid != p->pw_uid) {
	    GSendInt (V_NOHOME);
	    login_close(lc);
	    return;
	}
    }
# endif


/* restrict_time */
# ifdef USE_LOGIN_CAP
#  ifdef HAVE_AUTH_TIMEOK
    if (!auth_timeok(lc, time(NULL))) {
	GSendInt (V_BADTIME);
	login_close(lc);
	return;
    }
#  endif
# endif


/* restrict_expired; this MUST be the last one */
# if defined(HAVE_PW_EXPIRE) || defined(USESHADOW)

#  if !defined(HAVE_PW_EXPIRE) || (!defined(USE_LOGIN_CAP) && defined(USESHADOW))
    if (sp)
#  endif
    {

#  define DEFAULT_WARN  (2L * 7L)  /* Two weeks */

	tim = time(NULL) / 86400L;

#  ifdef USE_LOGIN_CAP
	quietlog = login_getcapbool(lc, "hushlogin", 0);
	warntime = login_getcaptime(lc, "warnexpire",
				    DEFAULT_WARN * 86400L, 
				    DEFAULT_WARN * 86400L) / 86400L;
#  else
	quietlog = 0;
#   ifdef USESHADOW
	warntime = sp->sp_warn != -1 ? sp->sp_warn : DEFAULT_WARN;
#   else
	warntime = DEFAULT_WARN;
#   endif
#  endif

	retv = V_OK;

#  ifdef HAVE_PW_EXPIRE
	expir = p->pw_expire / 86400L;
	if (expir) {
#  else
	if (sp->sp_expire != -1) {
	    expir = sp->sp_expire;
#  endif
	    if (tim > expir) {
		GSendInt (V_AEXPIRED);
#  ifdef USE_LOGIN_CAP
		login_close(lc);
#  endif
		return;
	    } else if (tim > (expir - warntime) && !quietlog) {
		expire = expir - tim;
		retv = V_AWEXPIRE;
	    }
	}

#  ifdef HAVE_PW_EXPIRE
	expir = p->pw_change / 86400L;
	if (expir) {
#  else
	if (sp->sp_max != -1) {
	    expir = sp->sp_lstchg + sp->sp_max;
#  endif
	    if (tim > expir) {
		GSendInt (V_PEXPIRED);
#  ifdef USE_LOGIN_CAP
		login_close(lc);
#  endif
		return;
	    } else if (tim > (expir - warntime) && !quietlog) {
		if (retv == V_OK || expire > expir) {
		    expire = expir - tim;
		    retv = V_PWEXPIRE;
		}
	    }
	}

	if (retv != V_OK) {
	    GSendInt (retv);
	    GSendInt (expire);
#  ifdef USE_LOGIN_CAP
	    login_close(lc);
#  endif
	    return;
	}

    }

# endif /* HAVE_PW_EXPIRE || USESHADOW */

    GSendInt (V_OK);
# ifdef USE_LOGIN_CAP
    login_close(lc);
# endif


#endif /* USE_PAM || AIXV3 */
}


static const char *envvars[] = {
    "TZ",			/* SYSV and SVR4, but never hurts */
#ifdef AIXV3
    "AUTHSTATE",		/* for kerberos */
#endif
#if defined(sony) && !defined(SYSTYPE_SYSV) && !defined(_SYSTYPE_SYSV)
    "bootdev",
    "boothowto",
    "cputype",
    "ioptype",
    "machine",
    "model",
    "CONSDEVTYPE",
    "SYS_LANGUAGE",
    "SYS_CODE",
#endif
#if (defined(SVR4) || defined(SYSV)) && defined(i386) && !defined(sun)
    "XLOCAL",
#endif
    NULL
};

static char **
userEnv (struct display *d, int isRoot, 
	 const char *user, const char *home, const char *shell)
{
    char	**env, *xma;

    env = defaultEnv (user);
    xma = 0;
    if (d->fifoPath && StrDup (&xma, d->fifoPath))
	if ((d->allowShutdown == SHUT_ALL ||
	     (d->allowShutdown == SHUT_ROOT && isRoot)) &&
	    StrApp (&xma, ",maysd", (char *)0))
	{
	    if (d->allowNuke == SHUT_ALL ||
		(d->allowNuke == SHUT_ROOT && isRoot))
		StrApp (&xma, ",mayfn", (char *)0);
	    StrApp (&xma, d->defSdMode == SHUT_FORCENOW ? ",fn" :
			  d->defSdMode == SHUT_TRYNOW ? ",tn" : ",sched", 
		    (char *)0);
	}
	if ((d->displayType & d_location) == dLocal && AnyReserveDisplays ())
	    StrApp (&xma, ",rsvd", (char *)0);
    if (xma)
    {
	env = setEnv (env, "XDM_MANAGED", xma);
	free (xma);
    }
    else
	env = setEnv (env, "XDM_MANAGED", "true");
    env = setEnv (env, "DISPLAY", d->name);
    env = setEnv (env, "HOME", home);
    env = setEnv (env, "PATH", isRoot ? d->systemPath : d->userPath);
    env = setEnv (env, "SHELL", shell);
#if !defined(USE_PAM) && !defined(AIXV3) && defined(KERBEROS)
    if (krbtkfile[0] != '\0')
	env = setEnv (env, "KRBTKFILE", krbtkfile);
#endif
    env = inheritEnv (env, envvars);
    return env;
}


static int
SetGid (const char *name, int gid)
{
    if (setgid(gid) < 0)
    {
	LogError("setgid(%d) (user %s) failed: %s\n",
		 gid, name, SysErrorMsg());
	return 0;
    }
#ifndef QNX4
    if (initgroups(name, gid) < 0)
    {
	LogError("initgroups for %s failed: %s\n", name, SysErrorMsg());
	return 0;
    }
#endif   /* QNX4 doesn't support multi-groups, no initgroups() */
    return 1;
}

static int
SetUid (const char *name, int uid)
{
    if (setuid(uid) < 0)
    {
	LogError("setuid(%d) (user %s) failed: %s\n",
		 uid, name, SysErrorMsg());
	return 0;
    }
    return 1;
}

static int
SetUser (const char *name, int uid, int gid)
{
    return SetGid (name, gid) && SetUid (name, uid);
}

static void
mergeSessionArgs (int cansave)
{
    char *mfname;
    const char *fname;
    int i, needsave;

    mfname = 0;
    fname = ".dmrc";
    if ((!curdmrc || newdmrc) && *dmrcDir)
	if (StrApp (&mfname, dmrcDir, "/", curuser, fname, 0))
	    fname = mfname;
    needsave = 0;
    if (!curdmrc) {
	curdmrc = iniLoad (fname);
	if (!curdmrc) {
	    StrDup (&curdmrc, "[Desktop]\nSession=default\n");
	    needsave = 1;
	}
    }
    if (newdmrc) {
	curdmrc = iniMerge (curdmrc, newdmrc);
	needsave = 1;
    }
    if (needsave && cansave)
	if (!iniSave (curdmrc, fname) && errno == ENOENT && mfname) {
	    for (i = 0; mfname[i]; i++)
		if (mfname[i] == '/') {
		    mfname[i] = 0;
		    mkdir (mfname, 0755);
		    mfname[i] = '/';
		}
	    iniSave (curdmrc, mfname);
	}
    if (mfname)
	free (mfname);
}

static int removeAuth;
static int sourceReset;

int
StartClient (struct display *d)
{
    const char	*shell, *home, *sessargs, *desksess;
    char	**argv, *fname, *str;
#ifdef USE_PAM
    char	**pam_env;
#else
# ifdef AIXV3
    char	*msg;
    char	**theenv;
    extern char	**newenv; /* from libs.a, this is set up by setpenv */
# endif
#endif
#ifdef HAS_SETUSERCONTEXT
    extern char	**environ;
#endif
    char	*failsafeArgv[2];
    int		i, pid;

#if defined(USE_PAM) || defined(AIXV3)
    if (!(p = getpwnam (curuser))) {
	LogError ("getpwnam(%s) failed.\n", curuser);
	return 0;
    }
#endif

#ifndef USE_PAM
# ifdef AIXV3
    msg = NULL;
    loginsuccess(curuser, hostname, tty, &msg);
    if (msg) {
	Debug("loginsuccess() - %s\n", msg);
	free((void *)msg);
    }
# else /* AIXV3 */
#  if defined(KERBEROS) && !defined(NO_AFS)
    if (krbtkfile[0] != '\0') {
	if (k_hasafs()) {
	    if (k_setpag() == -1)
		LogError ("setpag() for %s failed\n", curuser);
	    if ((ret = k_afsklog(NULL, NULL)) != KSUCCESS)
		LogError("AFS Warning: %s\n", krb_get_err_text(ret));
	}
    }
#  endif /* KERBEROS && AFS */
# endif /* AIXV3 */
#endif	/* !PAM */

    curuid = p->pw_uid;
    curgid = p->pw_gid;
    home = p->pw_dir;
    shell = p->pw_shell;
    userEnviron = userEnv (d, !curuid, curuser, home, shell);
    systemEnviron = systemEnv (d, curuser, home);
    Debug ("user environment:\n%[|''>'\n's"
	   "system environment:\n%[|''>'\n's"
	   "end of environments\n", 
	   userEnviron,
	   systemEnviron);

    /*
     * for user-based authorization schemes,
     * add the user to the server's allowed "hosts" list.
     */
    for (i = 0; i < d->authNum; i++)
    {
#ifdef SECURE_RPC
	if (d->authorizations[i]->name_length == 9 &&
	    memcmp(d->authorizations[i]->name, "SUN-DES-1", 9) == 0)
	{
	    XHostAddress	addr;
	    char		netname[MAXNETNAMELEN+1];
	    char		domainname[MAXNETNAMELEN+1];
    
	    getdomainname(domainname, sizeof domainname);
	    user2netname (netname, curuid, domainname);
	    addr.family = FamilyNetname;
	    addr.length = strlen (netname);
	    addr.address = netname;
	    XAddHost (dpy, &addr);
	}
#endif
#ifdef K5AUTH
	if (d->authorizations[i]->name_length == 14 &&
	    memcmp(d->authorizations[i]->name, "MIT-KERBEROS-5", 14) == 0)
	{
	    /* Update server's auth file with user-specific info.
	     * Don't need to AddHost because X server will do that
	     * automatically when it reads the cache we are about
	     * to point it at.
	     */
	    extern Xauth *Krb5GetAuthFor();

	    XauDisposeAuth (d->authorizations[i]);
	    d->authorizations[i] =
		Krb5GetAuthFor(14, "MIT-KERBEROS-5", d->name);
	    SaveServerAuthorizations (d, d->authorizations, d->authNum);
	}
#endif
    }

    /*
     * Run system-wide initialization file
     */
    sourceReset = 1;
    if (source (systemEnviron, d->startup) != 0) {
	LogError("Cannot execute startup script %\"s\n", d->startup);
	SessionExit (d, EX_NORMAL);
    }

    if (*dmrcDir)
	mergeSessionArgs (TRUE);

    Debug ("now starting the session\n");
#ifdef USE_PAM
    pam_open_session(pamh, 0);
    ReInitErrorLog ();
#endif    
    removeAuth = 1;
    if (d->fifoPath)
	chown (d->fifoPath, curuid, -1);
    endpwent();
#if !defined(USE_PAM) && !defined(AIXV3)
# ifndef QNX4  /* QNX4 doesn't need endspent() to end shadow passwd ops */
    endspent();
# endif
#endif
    switch (pid = Fork ()) {
    case 0:
	/* Do system-dependent login setup here */
#ifdef CSRG_BASED
	setsid();
#else
# if defined(SYSV) || defined(SVR4) || defined(__CYGWIN__)
#  if !(defined(SVR4) && defined(i386)) || defined(SCO325) || defined(__GNU__)
	setpgrp ();
#  endif
# else
	setpgrp (0, getpid ());
# endif
#endif

#if defined(USE_PAM) || !defined(AIXV3)

# ifndef HAS_SETUSERCONTEXT
	if (!SetGid (curuser, curgid))
	    exit (1);
# endif
# ifdef USE_PAM
	if (pam_setcred(pamh, 0) != PAM_SUCCESS) {
	    LogError("pam_setcred for %s failed: %s\n",
		     curuser, SysErrorMsg());
	    exit (1);
	}
	/* pass in environment variables set by libpam and modules it called */
#ifndef _AIX
	pam_env = pam_getenvlist(pamh);
#endif
	ReInitErrorLog ();
	if (pam_env)
	    for(; *pam_env; pam_env++)
		userEnviron = putEnv(*pam_env, userEnviron);
# endif
# ifndef HAS_SETUSERCONTEXT
#  if defined(BSD) && (BSD >= 199103)
	if (setlogin(curuser) < 0)
	{
	    LogError("setlogin for %s failed: %s\n", curuser, SysErrorMsg());
	    exit (1);
	}
#  endif
	if (!SetUid (curuser, curuid))
	    exit (1);
# else /* HAS_SETUSERCONTEXT */

	/*
	 * Destroy environment unless user has requested its preservation.
	 * We need to do this before setusercontext() because that may
	 * set or reset some environment variables.
	 */
	if (!(environ = initStrArr (0))) {
	    LogOutOfMem("StartSession");
	    exit (1);
	}

	/*
	 * Set the user's credentials: uid, gid, groups,
	 * environment variables, resource limits, and umask.
	 */
	if (setusercontext(NULL, p, p->pw_uid, LOGIN_SETALL) < 0)
	{
	    LogError("setusercontext for %s failed: %s\n",
		     curuser, SysErrorMsg());
	    exit (1);
	}

	for (i = 0; environ[i]; i++)
	    userEnviron = putEnv(environ[i], userEnviron);

# endif /* HAS_SETUSERCONTEXT */
#else /* AIXV3 */
	/*
	 * Set the user's credentials: uid, gid, groups,
	 * audit classes, user limits, and umask.
	 */
	if (setpcred(curuser, NULL) == -1)
	{
	    LogError("setpcred for %s failed: %s\n", curuser, SysErrorMsg());
	    exit (1);
	}

	/*
	 * Make a copy of the environment, because setpenv will trash it.
	 */
	if (!(theenv = xCopyStrArr (0, userEnviron)))
	{
	    LogOutOfMem("StartSession");
	    exit (1);
	}

	/*
	 * Set the users process environment. Store protected variables and
	 * obtain updated user environment list. This call will initialize
	 * global 'newenv'. 
	 */
	if (setpenv(curuser, PENV_INIT | PENV_ARGV | PENV_NOEXEC,
		    theenv, NULL) != 0)
	{
	    LogError("Can't set %s's process environment\n", curuser);
	    exit (1);
	}

	/*
	 * Free old userEnviron and replace with newenv from setpenv().
	 */
	free(theenv);
	freeStrArr(userEnviron);
	userEnviron = newenv;

#endif /* AIXV3 */

	/*
	 * for user-based authorization schemes,
	 * use the password to get the user's credentials.
	 */
#ifdef SECURE_RPC
	/* do like "keylogin" program */
	if (!curpass[0])
	    LogInfo("No password for NIS provided.\n");
	else
	{
	    char    netname[MAXNETNAMELEN+1], secretkey[HEXKEYBYTES+1];
	    int	    nameret, keyret;
	    int	    len;
	    int     key_set_ok = 0;

	    nameret = getnetname (netname);
	    Debug ("user netname: %s\n", netname);
	    len = strlen (curpass);
	    if (len > 8)
		bzero (curpass + 8, len - 8);
	    keyret = getsecretkey(netname, secretkey, curpass);
	    Debug ("getsecretkey returns %d, key length %d\n",
		   keyret, strlen (secretkey));
	    /* is there a key, and do we have the right password? */
	    if (keyret == 1)
	    {
		if (*secretkey)
		{
		    keyret = key_setsecret(secretkey);
		    Debug ("key_setsecret returns %d\n", keyret);
		    if (keyret == -1)
			LogError ("Failed to set NIS secret key\n");
		    else
			key_set_ok = 1;
		}
		else
		{
		    /* found a key, but couldn't interpret it */
		    LogError ("Password incorrect for NIS principal %s\n",
			      nameret ? netname : curuser);
		}
	    }
	    if (!key_set_ok)
	    {
		/* remove SUN-DES-1 from authorizations list */
		int i, j;
		for (i = 0; i < d->authNum; i++)
		{
		    if (d->authorizations[i]->name_length == 9 &&
			memcmp(d->authorizations[i]->name, "SUN-DES-1", 9) == 0)
		    {
			for (j = i+1; j < d->authNum; j++)
			    d->authorizations[j-1] = d->authorizations[j];
			d->authNum--;
			break;
		    }
		}
	    }
	    bzero(secretkey, strlen(secretkey));
	}
#endif
#ifdef K5AUTH
	/* do like "kinit" program */
	if (!curpass[0])
	    LogInfo("No password for Kerberos5 provided.\n");
	else
	{
	    int i, j;
	    int result;
	    extern char *Krb5CCacheName();

	    result = Krb5Init(curuser, curpass, d);
	    if (result == 0) {
		/* point session clients at the Kerberos credentials cache */
		userEnviron = setEnv(userEnviron,
				     "KRB5CCNAME", Krb5CCacheName(d->name));
	    } else {
		for (i = 0; i < d->authNum; i++)
		{
		    if (d->authorizations[i]->name_length == 14 &&
			memcmp(d->authorizations[i]->name, "MIT-KERBEROS-5", 14) == 0)
		    {
			/* remove Kerberos from authorizations list */
			for (j = i+1; j < d->authNum; j++)
			    d->authorizations[j-1] = d->authorizations[j];
			d->authNum--;
			break;
		    }
		}
	    }
	}
#endif /* K5AUTH */
	if (curpass)
	    bzero(curpass, strlen(curpass));
	SetUserAuthorization (d);
	home = getEnv (userEnviron, "HOME");
	if (home) {
	    if (chdir (home) < 0) {
		LogError ("Cannot chdir to %s's home %s: %s, using /\n",
			  curuser, home, SysErrorMsg());
		home = 0;
		userEnviron = setEnv(userEnviron, "HOME", "/");
		chdir ("/");
	    }
	} else
	    chdir ("/");
	if (!*dmrcDir)
	    mergeSessionArgs (home != 0);
	if (!(desksess = iniEntry (curdmrc, "Desktop", "Session", 0)))
	    desksess = "failsafe"; /* only due to OOM */
	userEnviron = setEnv (userEnviron, "DESKTOP_SESSION", desksess);
	for (i = 0; d->sessionsDirs[i]; i++) {
	    fname = 0;
	    if (StrApp (&fname, d->sessionsDirs[i], "/", desksess, ".desktop", 0)) {
		if ((str = iniLoad (fname))) {
		    if (!StrCmp (iniEntry (str, "Desktop Entry", "Hidden", 0), "true") ||
			!(sessargs = iniEntry (str, "Desktop Entry", "Exec", 0)))
			sessargs = "";
		    free (str);
		    free (fname);
		    goto gotit;
		}
		free (fname);
	    }
	}
	if (!strcmp (desksess, "failsafe") ||
	    !strcmp (desksess, "default") ||
	    !strcmp (desksess, "custom"))
	    sessargs = desksess;
	else
	    sessargs = "";
      gotit:
	argv = parseArgs ((char **)0, d->session);
	if (argv && argv[0] && *argv[0]) {
		argv = addStrArr (argv, sessargs, -1);
		Debug ("executing session %\"[s\n", argv);
		execute (argv, userEnviron);
		LogError ("Session %\"s execution failed: %s\n",
			  argv[0], SysErrorMsg());
	} else {
		LogError ("Session has no command/arguments\n");
	}
	failsafeArgv[0] = d->failsafeClient;
	failsafeArgv[1] = 0;
	execute (failsafeArgv, userEnviron);
	LogError ("Failsafe client %\"s execution failed: %s\n",
		  failsafeArgv[0], SysErrorMsg());
	exit (1);
    case -1:
	LogError ("Forking session on %s failed: %s\n",
		  d->name, SysErrorMsg());
	return 0;
    default:
	Debug ("StartSession, fork succeeded %d\n", pid);
/* ### right after forking dpy	mstrtalk.pipe = &d->pipe; */
#ifdef nofork_session
	if (!nofork_session)
#endif
	if (!Setjmp (mstrtalk.errjmp)) {
	    GSet (&mstrtalk);
	    GSendInt (D_User);
	    GSendInt (curuid);
	    if (d->autoReLogin) {
		GSendInt (D_ReLogin);
		GSendStr (curuser);
		GSendStr (curpass);
		GSendStr (newdmrc);
	    }
	}
	return pid;
    }
}

void
SessionExit (struct display *d, int status)
{
    /* make sure the server gets reset after the session is over */
    if (d->serverPid >= 2) {
	if (!d->terminateServer && d->resetSignal)
	    TerminateProcess (d->serverPid, d->resetSignal);
    } else
	ResetServer (d);
    if (sourceReset) {
	/*
	 * run system-wide reset file
	 */
	Debug ("source reset program %s\n", d->reset);
	source (systemEnviron, d->reset);
    }
    if (removeAuth)
    {
	if (d->fifoPath)
	    chown (d->fifoPath, 0, -1);
#ifdef USE_PAM
	if (pamh) {
	    /* shutdown PAM session */
	    if (pam_setcred(pamh, PAM_DELETE_CRED) != PAM_SUCCESS)
		LogError("pam_setcred(DELETE_CRED) for %s failed: %s\n",
			 curuser, SysErrorMsg());
	    pam_close_session(pamh, 0);
	    pam_end(pamh, PAM_SUCCESS);
	    pamh = NULL;
	    ReInitErrorLog ();
	}
#endif
	SetUser (curuser, curuid, curgid);
	RemoveUserAuthorization (d);
#ifdef K5AUTH
	/* do like "kdestroy" program */
	{
	    krb5_error_code code;
	    krb5_ccache ccache;

	    code = Krb5DisplayCCache(d->name, &ccache);
	    if (code)
		LogError("%s while getting Krb5 ccache to destroy\n",
			 error_message(code));
	    else {
		code = krb5_cc_destroy(ccache);
		if (code) {
		    if (code == KRB5_FCC_NOFILE)
			Debug ("no Kerberos ccache file found to destroy\n");
		    else
			LogError("%s while destroying Krb5 credentials cache\n",
				 error_message(code));
		} else
		    Debug ("kerberos ccache destroyed\n");
		krb5_cc_close(ccache);
	    }
	}
#endif /* K5AUTH */
#if !defined(USE_PAM) && !defined(AIXV3)
# ifdef KERBEROS
	if (krbtkfile[0]) {
	    (void) dest_tkt();
#  ifndef NO_AFS
	    if (k_hasafs())
		(void) k_unlog();
#  endif
	}
# endif
#endif /* !USE_PAM && !AIXV3*/
#ifdef USE_PAM
    } else {
	if (pamh) {
	    pam_end(pamh, PAM_SUCCESS);
	    pamh = NULL;
	    ReInitErrorLog ();
	}
#endif
    }
    Debug ("display %s exiting with status %d\n", d->name, status);
    exit (status);
}

int
ReadDmrc ()
{
    struct passwd *pw;
    char *data, *fname = 0;
    int len, pid, pfd[2], err;

    if (!curuser || !curuser[0] || !(pw = getpwnam (curuser)))
	return GE_NoUser;

    if (*dmrcDir) {
	if (!StrApp (&fname, dmrcDir, "/", curuser, ".dmrc", 0))
	    return GE_Error;
	if (!(curdmrc = iniLoad (fname))) {
	    free (fname);
	    return GE_Ok;
	}
	free (fname);
	return GE_NoFile;
    }

    if (!StrApp (&fname, pw->pw_dir, "/.dmrc", 0))
	return GE_Error;
    if ((curdmrc = iniLoad (fname))) {
	free (fname);
	return GE_Ok;
    }
    if (errno != EPERM) {
	free (fname);
	return GE_NoFile;
    }

    if (pipe (pfd))
	return GE_Error;
    if ((pid = Fork()) < 0) {
	close (pfd[0]);
	close (pfd[1]);
	return GE_Error;
    }
    if (!pid) {
	if (!SetUser (pw->pw_name, pw->pw_uid, pw->pw_gid))
	    exit (0);
	if (!(data = iniLoad (fname))) {
	    static const int m1 = -1;
	    write (pfd[1], &m1, sizeof(int));
	    exit (0);
	}
	len = strlen (data);
	write (pfd[1], &len, sizeof(int));
	write (pfd[1], data, len + 1);
	exit (0);
    }
    close (pfd[1]);
    free (fname);
    err = GE_Error;
    if (Reader (pfd[0], &len, sizeof(int)) == sizeof(int)) {
	if (len == -1)
	    err = GE_Denied;
	else if ((curdmrc = malloc(len + 1))) {
	    if (Reader (pfd[0], curdmrc, len + 1) == len + 1)
		err = GE_Ok;
	    else {
		free (curdmrc);
		curdmrc = 0;
	    }
	}
    }
    close (pfd[0]);
    (void) Wait4 (pid);
    return err;
}


#if (defined(Lynx) && !defined(HAS_CRYPT)) || (defined(SCO) && !defined(SCO_USA) && !defined(_SCO_DS))
char *crypt(const char *s1, const char *s2)
{
    return(s2);
}
#endif
