/*

Copyright 1988, 1998  The Open Group
Copyright 2000-2004 Oswald Buddenhagen <ossi@kde.org>

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
 * global xdm core declarations
 */

#ifndef _DM_H_
#define _DM_H_ 1

#include "greet.h"
#include <config.ci>

#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xmd.h>
#include <X11/Xauth.h>
#include <X11/Intrinsic.h>

#if defined(X_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE X_POSIX_C_SOURCE
# include <setjmp.h>
# include <limits.h>
# undef _POSIX_C_SOURCE
#else
# include <setjmp.h>
# include <limits.h>
#endif

#include <time.h>
#define Time_t time_t
#define TO_INF 0x7fffffff

#include <errno.h>

#ifdef XDMCP
# if defined(__osf__)
/* someone somewhere defines QUERY under Tru64 which confuses Xdmcp.h */
#  undef QUERY
# endif
# include <X11/Xdmcp.h>
#endif

#ifdef CSRG_BASED
# include <sys/param.h>
#endif
#ifdef pegasus
# undef dirty		/* Some bozo put a macro called dirty in sys/param.h */
#endif

#ifndef X_NOT_POSIX
# ifdef _POSIX_SOURCE
#  include <sys/wait.h>
# else
#  define _POSIX_SOURCE
#  ifdef SCO325
#   include <sys/procset.h>
#   include <sys/siginfo.h>
#  endif
#  include <sys/wait.h>
#  undef _POSIX_SOURCE
# endif
# define waitCode(w)	(WIFEXITED(w) ? WEXITSTATUS(w) : 0)
# define waitSig(w)	(WIFSIGNALED(w) ? WTERMSIG(w) : 0)
# ifdef WCOREDUMP
#  define waitCore(w)	(WCOREDUMP(w))
# else
#  define waitCore(w)	0	/* not in POSIX.  so what? */
# endif
typedef int		waitType;
#else /* X_NOT_POSIX */
# ifdef SYSV
#  define waitCode(w)	(((w) >> 8) & 0x7f)
#  define waitSig(w)	((w) & 0xff)
#  define waitCore(w)	(((w) >> 15) & 0x01)
typedef int		waitType;
# else /* SYSV */
#  include <sys/wait.h>
#  define waitCode(w)	((w).w_T.w_Retcode)
#  define waitSig(w)	((w).w_T.w_Termsig)
#  define waitCore(w)	((w).w_T.w_Coredump)
typedef union wait	waitType;
# endif
#endif /* X_NOT_POSIX */

#define waitCompose(sig,core,code) ((sig) * 256 + (core) * 128 + (code))
#define waitVal(w) waitCompose(waitSig(w), waitCore(w), waitCode(w))
#define WaitCode(w) ((w) & 0x7f)
#define WaitCore(w) (((w) >> 7) & 1)
#define WaitSig(w) (((w) >> 8) & 0xff)

#include <sys/time.h>
#define FD_TYPE	fd_set

#if defined(X_NOT_POSIX) || defined(__EMX__) || (defined(__NetBSD__) && defined(__sparc__))
# define Setjmp(e)	setjmp(e)
# define Longjmp(e,v)	longjmp(e,v)
# define Jmp_buf	jmp_buf
#else
# define Setjmp(e)	sigsetjmp(e,1)
# define Longjmp(e,v)	siglongjmp(e,v)
# define Jmp_buf	sigjmp_buf
#endif

#ifdef NEED_SIGNAL
# if defined(X_NOT_POSIX) && defined(SIGNALRETURNSINT)
#  define SIGVAL int
# else
#  define SIGVAL void
# endif
# if (defined(X_NOT_POSIX) && defined(SYSV)) || defined(__EMX__) || defined(ISC)
#  define SIGNALS_RESET_WHEN_CAUGHT
# endif
# ifdef X_POSIX_C_SOURCE
#  define _POSIX_C_SOURCE X_POSIX_C_SOURCE
#  include <signal.h>
#  undef _POSIX_C_SOURCE
# else
#  if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
#   include <signal.h>
#  else
#   define _POSIX_SOURCE
#   include <signal.h>
#   undef _POSIX_SOURCE
#  endif
# endif
#endif

typedef struct GPipe {
    int wfd, rfd;
    char *who;
} GPipe;

typedef struct GTalk {
    GPipe *pipe;
    Jmp_buf errjmp;
} GTalk;

typedef struct GProc {
    GPipe pipe;
    int pid;
} GProc;

typedef enum displayStatus { notRunning = 0, running, zombie, phoenix, raiser,
			     textMode, reserve, remoteLogin } DisplayStatus;

typedef enum serverStatus { ignore = 0, awaiting, starting,
			    terminated, killed, pausing } ServerStatus;

typedef struct RcStr {
    struct RcStr	*next;
    char		*str;
    int			cnt;
} RcStr;

typedef struct CfgDep {
    RcStr *name;
    long time;
} CfgDep;

typedef struct CfgArr {
	char		*data;		/* config value array; allocated */
	int		*idx;		/* config index array; alias */
	CfgDep		dep;		/* filestamp */
	int		numCfgEnt;	/* number of config entries */
} CfgArr;

struct bsock {
	int		fd;
	int		buflen;
	char		*buffer;
};

struct cmdsock {
	struct cmdsock	*next;
	struct bsock	sock;		/* buffered fd of the socket */
};

typedef struct {
	struct cmdsock	*css;		/* open connections */

	char		*path;		/* filename of the socket */
	int		fd;		/* fd of the socket */
	int		gid;		/* owner group of the socket */

	char		*fpath;		/* filename of the fifo */
	struct bsock	fifo;		/* buffered fd of the fifo */
} CtrlRec;

struct display {
	struct display	*next;
	struct disphist	*hstent;	/* display history entry */

	/* basic display information */
	char		*name;		/* DISPLAY name -- also referenced in hstent */
	char		*class2;	/* display class (may be NULL) */
	int		displayType;	/* location/origin/lifetime */
	CfgArr		cfg;		/* config data array */

	/* display state */
	DisplayStatus	status;		/* current status */
	int		zstatus;	/*  substatus while zombie */
	int		pid;		/* process id of child */
	int		serverPid;	/* process id of server (-1 if none) */
#ifdef HAVE_VTS
	int		reqSrvVT;	/* requested server VT (0 = none) */
	int		serverVT;	/* server VT (0 = none, -1 = pending) */
	struct display	*follower;	/* on exit, hand VT to this display */
#endif
	ServerStatus	serverStatus;	/* X server startup state */
	Time_t		lastStart;	/* time of last display start */
	int		startTries;	/* current start try */
	int		stillThere;	/* state during HUP processing */
	int		userSess;	/* -1=nobody, otherwise uid */
	char		*userName;
	CtrlRec		ctrl;		/* command socket & fifo */
	GPipe		pipe;		/* comm master <-> slave */
	GPipe		gpipe;		/* comm master <-> greeter */
#ifdef XDMCP
	char		*remoteHost;	/* for X -query type remote login */
	/* XDMCP state */
	unsigned	sessionID;	/* ID of active session */
	ARRAY8		peer;		/* display peer address */
	ARRAY8		from;		/* XDMCP port of display */
	unsigned	displayNumber;	/* numerical part of name */
	int		useChooser;	/* Run the chooser for this display */
	ARRAY8		clientAddr;	/* for chooser picking */
	unsigned	connectionType;	/* ... */
	int		xdmcpFd;
#endif

	CONF_CORE_LOCAL_DEFS
	
	int		idleTimeout;	/* abort login after that time */
	char		**serverArgv;	/* server program and arguments */
	char		*console;	/* the tty line hidden by the server */

	unsigned short	*authNameLens;	/* authorization protocol name lens */

	/* information potentially derived from resources */
	int		authNameNum;	/* number of protocol names */
	Xauth		**authorizations;/* authorization data */
	int		authNum;	/* number of authorizations */
	char		*authFile;	/* file to store authorization in */
};

typedef struct {
	unsigned	how:2,		/* 0=none 1=reboot 2=halt (SHUT_*) */
			force:2;
	int		uid;
	int		start;
	int		timeout;
} SdRec;

struct disphist {
	struct disphist	*next;
	char		*name;
	Time_t		lastExit;	/* time of last display exit */
	unsigned	rLogin:2,	/* 0=nothing 1=relogin 2=login */
			lock:1,		/* screen locker running */
			goodExit:1;	/* was the last exit "peaceful"? */
	SdRec		sdRec;
	char		*nuser, *npass, *nargs;
};

#ifdef XDMCP

#define PROTO_TIMEOUT	(30 * 60)   /* 30 minutes should be long enough */

struct protoDisplay {
	struct protoDisplay	*next;
	XdmcpNetaddr		address;   /* UDP address */
	int			addrlen;    /* UDP address length */
	unsigned long		date;	    /* creation date */
	CARD16			displayNumber;
	CARD16			connectionType;
	ARRAY8			connectionAddress;
	CARD32			sessionID;
	Xauth			*fileAuthorization;
	Xauth			*xdmcpAuthorization;
	ARRAY8			authenticationName;
	ARRAY8			authenticationData;
	XdmAuthKeyRec		key;
};
#endif /* XDMCP */

/* status code for RStopDisplay */
#define DS_RESTART	0
#define DS_TEXTMODE	1
#define DS_RESERVE	2
#define DS_REMOTE	3
#define DS_REMOVE	4

/* command codes dpy process -> master process */
#define D_User		1
#define D_ReLogin	2
#define D_ChooseHost	4
#define D_RemoteHost	5
#define D_XConnOk	6

extern int	debugLevel;

CONF_CORE_GLOBAL_DECLS

/* in daemon.c */
void BecomeDaemon (void);

/* in dm.c */
extern char *prog, *progpath;
extern time_t now;
extern SdRec sdRec;
void StartDisplay (struct display *d);
void StartDisplayP2 (struct display *d);
void StopDisplay (struct display *d);
void SetTitle (const char *name);
void SwitchToX (struct display *d);
void setNLogin (struct display *d, 
		const char *nuser, const char *npass, char *nargs,
		int rl);
void cancelShutdown (void);

/* in ctrl.c */
void openCtrl (struct display *d);
void closeCtrl (struct display *d);
int handleCtrl (FD_TYPE *reads, struct display *d);
void chownCtrl (CtrlRec *cr, int uid, int gid);
void updateCtrl (void);

/* in dpylist.c */
extern struct display *displays;	/* that's ugly ... */
int AnyDisplaysLeft (void);
void ForEachDisplay (void (*f)(struct display *));
#ifdef HAVE_VTS
void ForEachDisplayRev (void (*f)(struct display *));
#endif
void RemoveDisplay (struct display *old);
struct display
	*FindDisplayByName (const char *name),
#ifdef XDMCP
	*FindDisplayBySessionID (CARD32 sessionID),
	*FindDisplayByAddress (XdmcpNetaddr addr, int addrlen, CARD16 displayNumber),
#endif /* XDMCP */
	*FindDisplayByPid (int pid),
	*FindDisplayByServerPid (int serverPid),
	*NewDisplay (const char *name);
int AnyActiveDisplays (void);
int AnyRunningDisplays (void);
int AnyReserveDisplays (void);
int idleReserveDisplays (void);
int AllLocalDisplaysLocked (struct display *dp);
int StartReserveDisplay (int lt);
void ReapReserveDisplays (void);

/* in reset.c */
void pseudoReset (void);

/* in resource.c */
char **FindCfgEnt (struct display *d, int id);
int InitResources (char **argv);
int LoadDMResources (int force);
int LoadDisplayResources (struct display *d);
void ScanServers (int force);
void CloseGetter (void);
int startConfig (int what, CfgDep *dep, int force);
RcStr *newStr (char *str);
void delStr (RcStr *str);
extern GTalk cnftalk;

/* in session.c */
extern struct display *td;
extern const char *td_setup;
char **baseEnv (const char *user);
char **inheritEnv (char **env, const char **what);
char **systemEnv (const char *user);
int source (char **env, const char *file, const char *arg);
void ManageSession (struct display *d);

extern GTalk mstrtalk, grttalk;
extern GProc grtproc;
void OpenGreeter (void);
int CloseGreeter (int force);
int CtrlGreeterWait (int wreply);
void PrepErrorGreet (void);
char *conv_interact (int what, const char *prompt);

/* process.c */
#include <stdlib.h>

#ifdef NEED_SIGNAL
typedef SIGVAL (*SIGFUNC)(int);

SIGVAL (*Signal(int, SIGFUNC Handler))(int);
#endif

void RegisterInput (int fd);
void UnregisterInput (int fd);
void RegisterCloseOnFork (int fd);
void CloseNClearCloseOnFork (int fd);
int Fork (void);
int Wait4 (int pid);
void execute (char **argv, char **env);
int runAndWait (char **args, char **env);
void TerminateProcess (int pid, int sig);

void GSet (GTalk *talk);	/* call before GOpen! */
int GFork (GPipe *pajp, const char *pname, char *cname,
	   GPipe *ogp, char *cgname);
void GClosen (GPipe *pajp);
int GOpen (GProc *proc,
	   char **argv, const char *what, char **env, char *cname,
	   GPipe *gp);
int GClose (GProc *proc, GPipe *gp, int force);

void GSendInt (int val);
int GRecvInt (void);
int GRecvCmd (int *cmd);
void GSendArr (int len, const char *data);
char *GRecvArr (int *len);
int GRecvStrBuf (char *buf);
int GRecvArrBuf (char *buf);
void GSendStr (const char *buf);
void GSendNStr (const char *buf, int len);
char *GRecvStr (void);
void GSendArgv (char **argv);
void GSendStrArr (int len, char **data);
char **GRecvStrArr (int *len);
char **GRecvArgv (void);

/* client.c */
#define GCONV_NORMAL	0
#define GCONV_HIDDEN	1
#define GCONV_USER	2
#define GCONV_PASS	3
#define GCONV_PASS_ND	4
#define GCONV_BINARY	5
typedef char * (*GConvFunc) (int what, const char *prompt);
int Verify (GConvFunc gconv, int rootok);
int StartClient (void);
void SessionExit (int status) ATTR_NORETURN;
int ReadDmrc (void);
extern char **userEnviron, **systemEnviron;
extern char *curuser, *curpass, *curtype, *newpass,
	*dmrcuser, *curdmrc, *newdmrc;

/* server.c */
void StartServer (struct display *d);
void AbortStartServer (struct display *d);
void StartServerSuccess (void);
void StartServerFailed (void);
void StartServerTimeout (void);
extern struct display *startingServer;
extern time_t serverTimeout;

void WaitForServer (struct display *d);
void ResetServer (struct display *d);
int PingServer(struct display *d);
extern Display *dpy;

/* in util.c */
void *Calloc (size_t nmemb, size_t size);
void *Malloc (size_t size);
void *Realloc (void *ptr, size_t size);
void WipeStr (char *str);
int StrCmp (const char *s1, const char *s2);
int StrNDup (char **dst, const char *src, int len);
int StrDup (char **dst, const char *src);
int arrLen (char **arr);
void freeStrArr (char **arr);
char **initStrArr (char **arr);
char **xCopyStrArr (int rn, char **arr);
/* Note: the following functions free the old data even in case of failure */
int ReStrN (char **dst, const char *src, int len);
int ReStr (char **dst, const char *src);
int StrApp (char **dst, ...);
char **addStrArr (char **arr, const char *str, int len);
char **parseArgs (char **argv, const char *string);
/* End note */
char **setEnv (char **e, const char *name, const char *value);
char **putEnv (const char *string, char **env);
const char *getEnv (char **e, const char *name);
const char *localHostname (void);
int Reader (int fd, void *buf, int len);
int Writer (int fd, const void *buf, int len);

/* in inifile.c */
char *iniLoad (const char *fname);
int iniSave (const char *data, const char *fname);
char *iniEntry (char *data, const char *section, const char *key, const char *value);
char *iniMerge (char *data, const char *newdata);

#ifdef XDMCP

/* in xdmcp.c */
char *NetworkAddressToHostname (CARD16 connectionType, ARRAY8Ptr connectionAddress);
void SendFailed (struct display *d, const char *reason);
void init_session_id(void);

/* in netaddr.c */
char *NetaddrAddress(XdmcpNetaddr netaddrp, int *lenp);
char *NetaddrPort(XdmcpNetaddr netaddrp, int *lenp);
int ConvertAddr (XdmcpNetaddr saddr, int *len, char **addr);
int NetaddrFamily (XdmcpNetaddr netaddrp);
int addressEqual (XdmcpNetaddr a1, int len1, XdmcpNetaddr a2, int len2);

/* in policy.c */
struct sockaddr;
ARRAY8Ptr Accept (struct sockaddr *from, int fromlen, CARD16 displayNumber);
ARRAY8Ptr ChooseAuthentication (ARRAYofARRAY8Ptr authenticationNames);
int CheckAuthentication (struct protoDisplay *pdpy, ARRAY8Ptr displayID, ARRAY8Ptr name, ARRAY8Ptr data);
int SelectAuthorizationTypeIndex (ARRAY8Ptr authenticationName, ARRAYofARRAY8Ptr authorizationNames);
int SelectConnectionTypeIndex (ARRAY16Ptr connectionTypes, ARRAYofARRAY8Ptr connectionAddresses);
int Willing (ARRAY8Ptr addr, CARD16 connectionType, ARRAY8Ptr authenticationName, ARRAY8Ptr status, xdmOpCode type);

/* in protodpy.c */
void DisposeProtoDisplay(struct protoDisplay *pdpy);

struct protoDisplay	*FindProtoDisplay (
					XdmcpNetaddr address,
					int          addrlen,
					CARD16       displayNumber);
struct protoDisplay	*NewProtoDisplay (
					XdmcpNetaddr address,
					int	     addrlen,
					CARD16	     displayNumber,
					CARD16	     connectionType,
					ARRAY8Ptr    connectionAddress,
					CARD32	     sessionID);

#define FamilyBroadcast 0xffff
typedef void (*ChooserFunc)(CARD16 connectionType, ARRAY8Ptr addr, char *closure);
typedef void (*ListenFunc)(ARRAY8Ptr addr, void **closure);

/* in access.c */
ARRAY8Ptr getLocalAddress (void);
int AcceptableDisplayAddress (ARRAY8Ptr clientAddress, CARD16 connectionType, xdmOpCode type);
int ForEachMatchingIndirectHost (ARRAY8Ptr clientAddress, CARD16 connectionType, ChooserFunc function, char *closure);
void ScanAccessDatabase (int force);
int UseChooser (ARRAY8Ptr clientAddress, CARD16 connectionType);
void ForEachChooserHost (ARRAY8Ptr clientAddress, CARD16 connectionType, ChooserFunc function, char *closure);
void ForEachListenAddr (ListenFunc listenfunction, ListenFunc mcastfcuntion, void **closure);

/* in choose.c */
ARRAY8Ptr IndirectChoice (ARRAY8Ptr clientAddress, CARD16 connectionType);
int IsIndirectClient (ARRAY8Ptr clientAddress, CARD16 connectionType);
int RememberIndirectClient (ARRAY8Ptr clientAddress, CARD16 connectionType);
void ForgetIndirectClient ( ARRAY8Ptr clientAddress, CARD16 connectionType);
int RegisterIndirectChoice (ARRAY8Ptr clientAddress, CARD16 connectionType, ARRAY8Ptr choice);
int DoChoose (void);

/* socket.c or streams.c */
void UpdateListenSockets (void);
int AnyListenSockets (void);
int ProcessListenSockets (FD_TYPE *reads);

/* in xdmcp.c */
void ProcessRequestSocket (int fd);

#endif /* XDMCP */

/* automatically fork off reserve display if all displays are locked */
/*#define AUTO_RESERVE 1*/

#endif /* _DM_H_ */
