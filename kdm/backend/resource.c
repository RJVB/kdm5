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
 * obtain configuration data
 */

#include "dm.h"
#include "dm_error.h"

#include <sys/stat.h>


static char **originalArgv;

static GProc getter;
GTalk cnftalk;

static void
OpenGetter ()
{
    GSet (&cnftalk);
    if (!getter.pid) {
	if (GOpen (&getter,
		   originalArgv, "_config", 0, strdup("config reader"),
		   0))
	    LogPanic ("Cannot run config reader\n");
	Debug ("getter now ready\n");
    }
}

void
CloseGetter ()
{
    if (getter.pid) {
	GSet (&cnftalk);
	(void) GClose (&getter, 0, 0);
	Debug ("getter now closed\n");
    }
}

/*
 * ref-counted, unique-instance strings
 */
static RcStr *strs;

/*
 * make a ref-counted string of the argument. the new string will
 * have a ref-count of 1. the passed string pointer is no longer valid.
 */
RcStr *
newStr (char *str)
{
    RcStr *cs;

    for (cs = strs; cs; cs = cs->next)
	if (!strcmp (str, cs->str)) {
	    free (str);
	    cs->cnt++;
	    return cs;
	}
    if (!(cs = Malloc (sizeof (*cs))))
	return 0;
    cs->cnt = 1;
    cs->str = str;
    cs->next = strs;
    strs = cs;
    return cs;
}

/*
 * decrement ref-count and delete string when count drops to 0.
 */
void
delStr (RcStr *str)
{
    RcStr **cs;

    if (!str || --str->cnt)
	return;
    for (cs = &strs; *cs; cs = &((*cs)->next))
	if (str == *cs) {
	    *cs = (*cs)->next;
	    free ((*cs)->str);
	    free (*cs);
	    break;
	}
}


static long
mTime (const char *fn)
{
    struct stat st;

    if (stat (fn, &st))
	return -1;
    else
	return st.st_mtime;
}

typedef struct CfgFile {
    RcStr *name;
    int depidx;
    long deptime;
} CfgFile;

static int numCfgFiles;
static CfgFile *cfgFiles;

static int cfgMapT[] = {
    GC_gGlobal,
    GC_gDisplay,
#ifdef XDMCP
    GC_gXaccess,
#endif
    GC_gXservers
};
static int cfgMap[as(cfgMapT)];

static int
GetDeps ()
{
    int ncf, i, dep, ret;
    CfgFile *cf;

    OpenGetter ();
    GSendInt (GC_Files);
    ncf = GRecvInt ();
    if (!(cf = Malloc (ncf * sizeof (*cf)))) {
	CloseGetter ();
	return 0;
    }
    for (i = 0; i < ncf; i++) {
	cf[i].name = newStr (GRecvStr ());
	if ((dep = cf[i].depidx = GRecvInt ()) != -1)
	    cf[i].deptime = mTime (cf[dep].name->str);
    }
    if (cfgFiles) {
	for (i = 0; i < numCfgFiles; i++)
	    delStr (cfgFiles[i].name);
	free (cfgFiles);
    }
    ret = 1;
    cfgFiles = cf;
    numCfgFiles = ncf;
    for (i = 0; i < as(cfgMapT); i++) {
	GSendInt (cfgMapT[i]);
	if ((cfgMap[i] = GRecvInt ()) < 0) {
	    LogError ("Config reader does not support config cathegory %#x\n", 
		      cfgMapT[i]);
	    ret = 0;
	}
    }
    GSendInt (-1);
    return ret;
}

static int
checkDep (int idx)
{
    int dep;

    if ((dep = cfgFiles[idx].depidx) == -1)
	return 0;
    if (checkDep (dep))
	return 1;
    return mTime (cfgFiles[dep].name->str) != cfgFiles[idx].deptime;
}

static int
needsReScan (int what, CfgDep *dep)
{
    int idx;
    long mt;

    for (idx = 0; idx < as(cfgMap); idx++)
	if (cfgMapT[idx] == what)
	    break;
    idx = cfgMap[idx];
    if (checkDep (idx))
	if (!GetDeps ())
	    return 0;
    mt = mTime (cfgFiles[idx].name->str);
    if (dep->name != cfgFiles[idx].name) {
	if (dep->name)
	    delStr (dep->name);
	dep->name = cfgFiles[idx].name;
	dep->name->cnt++;
	dep->time = mt;
	return 1;
    } else if (dep->time != mt) {
	dep->time = mt;
	return 1;
    } else
	return 0;
}

int
startConfig (int what, CfgDep *dep, int force)
{
    if (!needsReScan (what, dep) && !force)
	return 0;
    OpenGetter ();
    GSendInt (GC_GetConf);
    GSendInt (what);
    GSendStr (dep->name->str);
    return 1;
}

static void
LoadResources (CfgArr *conf)
{
    char **vptr, **pptr, *cptr;
    int *iptr, i, id, nu, j, nptr, nint, nchr;

    if (conf->data)
	free (conf->data);
    conf->numCfgEnt = GRecvInt ();
    nptr = GRecvInt ();
    nint = GRecvInt ();
    nchr = GRecvInt ();
    if (!(conf->data = Malloc (conf->numCfgEnt * (sizeof(int) + sizeof(char *))
			       + nptr * sizeof(char *)
			       + nint * sizeof(int)
			       + nchr))) {
	CloseGetter ();
	return;
    }
    vptr = (char **)conf->data;
    pptr = vptr + conf->numCfgEnt;
    conf->idx = (int *)(pptr + nptr);
    iptr = conf->idx + conf->numCfgEnt;
    cptr = (char *)(iptr + nint);
    for (i = 0; i < conf->numCfgEnt; i++) {
	id = GRecvInt ();
	conf->idx[i] = id;
	switch (id & C_TYPE_MASK) {
	case C_TYPE_INT:
	    vptr[i] = (char *)((unsigned long)GRecvInt ());
	    break;
	case C_TYPE_STR:
	    vptr[i] = cptr;
	    cptr += GRecvStrBuf (cptr);
	    break;
	case C_TYPE_ARGV:
	    nu = GRecvInt ();
	    vptr[i] = (char *)pptr;
	    for (j = 0; j < nu; j++) {
		*pptr++ = cptr;
		cptr += GRecvStrBuf (cptr);
	    }
	    *pptr++ = (char *)0;
	    break;
	default:
	    LogError ("Config reader supplied unknown data type in id %#x\n", 
		      id);
	    break;
	}
    }
}

static void
ApplyResource (int id, char **src, char **dst)
{
    switch (id & C_TYPE_MASK) {
    case C_TYPE_INT:
	*(int *)dst = *(int *)src;
	break;
    case C_TYPE_STR:
    case C_TYPE_ARGV:
	*dst = *src;
	break;
    }
}


#define boffset(f)	XtOffsetOf(struct display, f)

/* no global variables exported currently
struct globEnts {
	int	id;
	char	**off;
} globEnt[] = {
};
 */

/* no per-display variables exported currently
struct dpyEnts {
	int	id;
	int	off;
} dpyEnt[] = {
};
 */

CfgArr cfg;

char **
FindCfgEnt (struct display *d, int id)
{
    int i;

/* no global variables exported currently
    for (i = 0; i < as(globEnt); i++)
	if (globEnt[i].id == id)
	    return globEnt[i].off;
 */
    for (i = 0; i < cfg.numCfgEnt; i++)
	if (cfg.idx[i] == id)
	    return ((char **)cfg.data) + i;
    if (d) {
/* no per-display variables exported currently
	for (i = 0; i < as(dpyEnt); i++)
	    if (dpyEnt[i].id == id)
		return (char **)(((char *)d) + dpyEnt[i].off);
 */
	for (i = 0; i < d->cfg.numCfgEnt; i++)
	    if (d->cfg.idx[i] == id)
		return ((char **)d->cfg.data) + i;
    }
    Debug ("unknown config entry %#x requested\n", id);
    return (char **)0;
}	    


CONF_CORE_GLOBAL_DEFS

struct globVals {
	int	id;
	char	**off;
} globVal[] = {
CONF_CORE_GLOBALS
};

int
LoadDMResources (int force)
{
    int		i, ret;
    char	**ent;

    if (Setjmp (cnftalk.errjmp))
	return 0;	/* may memleak, but we probably have to abort anyway */
    if (!startConfig (GC_gGlobal, &cfg.dep, force))
	return 1;
    LoadResources (&cfg);
/*    Debug ("manager resources: %[*x\n", 
	    cfg.numCfgEnt, ((char **)cfg.data) + cfg.numCfgEnt);*/
    ret = 1;
    for (i = 0; i < as(globVal); i++) {
	if (!(ent = FindCfgEnt (0, globVal[i].id)))
	    ret = 0;
	else
	    ApplyResource (globVal[i].id, ent, globVal[i].off);
    }
    if (!ret)
	LogError ("Internal error: config reader supplied incomplete data\n");
    return ret;
}


struct dpyVals {
	int	id;
	int	off;
} dpyVal[] = {
CONF_CORE_LOCALS
};

int
LoadDisplayResources (struct display *d)
{
    int		i, ret;
    char	**ent;

    if (Setjmp (cnftalk.errjmp))
	return 0;	/* may memleak */
    if (!startConfig (GC_gDisplay, &d->cfg.dep, FALSE))
	return 1;
    GSendStr (d->name);
    GSendStr (d->class2);
    LoadResources (&d->cfg);
/*    Debug ("display(%s, %s) resources: %[*x\n", d->name, d->class2,
	    d->cfg.numCfgEnt, ((char **)d->cfg.data) + d->cfg.numCfgEnt);*/
    ret = 1;
    for (i = 0; i < as(dpyVal); i++) {
	if (!(ent = FindCfgEnt (d, dpyVal[i].id)))
	    ret = 0;
	else
	    ApplyResource (dpyVal[i].id, ent, 
			   (char **)(((char *)d) + dpyVal[i].off));
    }
    if (!ret)
	LogError ("Internal error: config reader supplied incomplete data\n");
    return ret;
}

int
InitResources (char **argv)
{
    originalArgv = argv;
    cnftalk.pipe = &getter.pipe;
    if (Setjmp (cnftalk.errjmp))
	return 0;	/* may memleak */
    return GetDeps ();
}

void
ScanServers (int force)
{
    char		*name, *class2, *console, **argv;
    const char		*dtx;
    struct display	*d;
    int			nserv, type;
    static CfgDep	xsDep;

    Debug("ScanServers\n");
    if (Setjmp (cnftalk.errjmp))
	return;	/* may memleak */
    if (!startConfig (GC_gXservers, &xsDep, force))
	return;
    nserv = GRecvInt ();
    while (nserv--) {
	name = GRecvStr ();
	class2 = GRecvStr ();
	console = GRecvStr ();
	type = GRecvInt ();
	argv = GRecvArgv ();
	if ((d = FindDisplayByName (name)))
	{
	    if (d->class2)
		free (d->class2);
	    if (d->console)
		free (d->console);
	    freeStrArr (d->serverArgv);
	    dtx = "existing";
	}
	else
	{
	    d = NewDisplay (name);
	    dtx = "new";
	}
	d->stillThere = 1;
	Debug ("found %s display: %s %s %s%s %[s\n",
	       dtx, d->name, d->class2, 
	       ((type & d_location) == dLocal) ? "local" : "foreign",
	       ((type & d_lifetime) == dReserve) ? " reserve" : "", argv);
	d->class2 = class2;
	d->console = console;
	d->serverArgv = argv;
#ifdef HAVE_VTS
	for (; argv[0]; argv++)
	    if (argv[0][0] == 'v' && argv[0][1] == 't')
		d->reqSrvVT = atoi (argv[0] + 2);
#endif
	d->displayType = type;
	if ((type & d_lifetime) == dReserve && d->status == notRunning)
	    d->status = reserve;
	else if ((type & d_lifetime) != dReserve && d->status == reserve)
	    d->status = notRunning;
	free (name);
    }
}

