/*

Copyright 1989, 1998  The Open Group
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
 * Author: Keith Packard, MIT X Consortium
 *
 * various utility routines
 */

#include "dm.h"
#include "dm_error.h"

#include <X11/Xosdefs.h>
#ifndef X_NOT_STDC_ENV
# include <string.h>
# include <unistd.h>
#endif

#ifdef USG
# define NEED_UTSNAME
#endif

#ifdef NEED_UTSNAME
# include <sys/utsname.h>
#endif

void *
Calloc( size_t nmemb, size_t size )
{
	void *ret;

	if (!(ret = calloc( nmemb, size )))
		LogOutOfMem();
	return ret;
}

void *
Malloc( size_t size )
{
	void *ret;

	if (!(ret = malloc( size )))
		LogOutOfMem();
	return ret;
}

void *
Realloc( void *ptr, size_t size )
{
	void *ret;

	if (!(ret = realloc( ptr, size )) && size)
		LogOutOfMem();
	return ret;
}

int
StrCmp( const char *s1, const char *s2 )
{
	if (s1 == s2)
		return 0;
	if (!s1)
		return -1;
	if (!s2)
		return 1;
	return strcmp( s1, s2 );
}

void
WipeStr( char *str )
{
	if (str) {
		bzero( str, strlen( str ) );
		free( str );
	}
}

/* duplicate src; wipe & free old dst string */
int
ReStrN( char **dst, const char *src, int len )
{
	char *ndst = 0;

	if (src) {
		if (len < 0)
			len = strlen( src );
		if (*dst && !memcmp( *dst, src, len ) && !(*dst)[len])
			return 1;
		if (!(ndst = Malloc( len + 1 ))) {
			WipeStr( *dst );
			*dst = 0;
			return 0;
		}
		memcpy( ndst, src, len );
		ndst[len] = 0;
	}
	WipeStr( *dst ); /* make an option, if we should become heavily used */
	*dst = ndst;
	return 2;
}

int
ReStr( char **dst, const char *src )
{
	return ReStrN( dst, src, -1 );
}

/* duplicate src */
int
StrNDup( char **dst, const char *src, int len )
{
	if (src) {
		if (len < 0)
			len = strlen( src );
		if (!(*dst = Malloc( len + 1 )))
			return 0;
		memcpy( *dst, src, len );
		(*dst)[len] = 0;
	} else
		*dst = 0;
	return 1;
}

int
StrDup( char **dst, const char *src )
{
	return StrNDup( dst, src, -1 );
}

/* append any number of strings to dst */
int
StrApp( char **dst, ... )
{
	int len;
	char *bk, *pt, *dp;
	va_list va;

	len = 1;
	if (*dst)
		len += strlen( *dst );
	va_start( va, dst );
	for (;;) {
		pt = va_arg( va, char * );
		if (!pt)
			break;
		len += strlen( pt );
	}
	va_end( va );
	if (!(bk = Malloc( len ))) {
		if (*dst) {
			free( *dst );
			*dst = 0;
		}
		return 0;
	}
	dp = bk;
	if (*dst) {
		len = strlen( *dst );
		memcpy( dp, *dst, len );
		dp += len;
		free( *dst );
	}
	va_start( va, dst );
	for (;;) {
		pt = va_arg( va, char * );
		if (!pt)
			break;
		len = strlen( pt );
		memcpy( dp, pt, len );
		dp += len;
	}
	va_end( va );
	*dp = '\0';
	*dst = bk;
	return 1;
}


char **
initStrArr( char **arr )
{
	if (!arr && (arr = Malloc( sizeof(char *))))
		arr[0] = 0;
	return arr;
}

int
arrLen( char **arr )
{
	int nu = 0;
	if (arr)
		for (; arr[nu]; nu++);
	return nu;
}

char **
addStrArr( char **arr, const char *str, int len )
{
	char **rarr;
	int nu;

	nu = arrLen( arr );
	if ((rarr = Realloc( arr, sizeof(char *)* (nu + 2) ))) {
		rarr[nu + 1] = 0;
		if (StrNDup( rarr + nu, str, len ))
			return rarr;
		freeStrArr( rarr );
		return 0;
	}
	freeStrArr( arr );
	return 0;
}

char **
xCopyStrArr( int rn, char **arr )
{
	char **rarr;
	int nu;

	nu = arrLen( arr );
	if ((rarr = Calloc( sizeof(char *), nu + rn + 1 )))
		memcpy( rarr + rn, arr, sizeof(char *)* nu );
	return rarr;
}

void
freeStrArr( char **arr )
{
	char **a;

	if (arr) {
		for (a = arr; *a; a++)
			free( *a );
		free( arr );
	}
}


char **
parseArgs( char **argv, const char *string )
{
	const char *word;
	char ch;

	if (!(argv = initStrArr( argv )))
		return 0;
	for (word = string; ; ++string) {
		ch = *string;
		if (!ch || ch == ' ' || ch == '\t') {
			if (word != string)
				if (!(argv = addStrArr( argv, word, string - word )))
					return 0;
			if (!ch)
				return argv;
			word = string + 1;
		}
	}
}


const char *
getEnv( char **e, const char *name )
{
	if (e) {
		int l = strlen( name );
		for (; *e; e++)
			if (!memcmp( *e, name, l ) && (*e)[l] == '=')
				return (*e) + l + 1;
	}
	return 0;
}

char **
setEnv( char **e, const char *name, const char *value )
{
	char **new, **old;
	char *newe;
	int envsize;
	int l;

#ifdef AIXV3
	/* setpenv() depends on "SYSENVIRON:", not "SYSENVIRON:=" */
	if (!value) {
		if (!StrDup( &newe, name ))
			return e;
	} else
#endif
	{
		newe = 0;
		if (!StrApp( &newe, name, "=", value, (char *)0 ))
			return e;
	}
	envsize = 0;
	if (e) {
		l = strlen( name );
		for (old = e; *old; old++)
			if (!memcmp( *old, name, l ) && ((*old)[l] == '=' || !(*old)[l]))
			{
				free( *old );
				*old = newe;
				return e;
			}
		envsize = old - e;
	}
	if (!(new = (char **)
	      Realloc( (char *)e, (unsigned)((envsize + 2) * sizeof(char *)) )))
	{
		free( newe );
		return e;
	}
	new[envsize] = newe;
	new[envsize + 1] = 0;
	return new;
}

char **
putEnv( const char *string, char **env )
{
	char *b, *n;

	if (!(b = strchr( string, '=' )))
		return NULL;
	if (!StrNDup( &n, string, b - string ))
		return NULL;
	env = setEnv( env, n, b + 1 );
	free( n );
	return env;
}

static int
GetHostname( char *buf, int maxlen )
{
	int len;

#ifdef NEED_UTSNAME
	/*
	 * same host name crock as in server and xinit.
	 */
	struct utsname name;

	uname( &name );
	len = strlen( name.nodename );
	if (len >= maxlen) len = maxlen - 1;
	memcpy( buf, name.nodename, len );
	buf[len] = '\0';
#else
	buf[0] = '\0';
	(void)gethostname( buf, maxlen );
	buf[maxlen - 1] = '\0';
	len = strlen( buf );
#endif /* hpux */
	return len;
}

static char localHostbuf[256];
static int gotLocalHostname;

const char *
localHostname( void )
{
	if (!gotLocalHostname)
	{
		GetHostname( localHostbuf, sizeof(localHostbuf) - 1 );
		gotLocalHostname = 1;
	}
	return localHostbuf;
}

static int
AtomicIO( ssize_t (*f)( int, void *, size_t ), int fd, void *buf, int count )
{
	int ret, rlen;

	for (rlen = 0; rlen < count; ) {
	  dord:
		ret = f( fd, (void *)((char *)buf + rlen), count - rlen );
		if (ret < 0) {
			if (errno == EINTR)
				goto dord;
			if (errno == EAGAIN)
				break;
			return -1;
		}
		if (!ret)
			break;
		rlen += ret;
	}
	return rlen;
}

int
Reader( int fd, void *buf, int count )
{
	return AtomicIO( read, fd, buf, count );
}

int
Writer( int fd, const void *buf, int count )
{
	return AtomicIO( (ssize_t(*)( int, void *, size_t ))write,
	                 fd, (void *)buf, count );
}
