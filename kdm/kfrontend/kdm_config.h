    /*

    Config option definitions private to KDM

    Copyright (C) 2001-2002 Oswald Buddenhagen <ossi@kde.org>


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
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

    */

#ifndef _KDM_CONFIG_H_
#define _KDM_CONFIG_H_

#include <config.h>

#include <greet.h>

#define C_grabServer		(C_TYPE_INT | 0x1000)
#define C_grabTimeout		(C_TYPE_INT | 0x1001)	
#define C_authComplain		(C_TYPE_INT | 0x1002)

#define C_useLilo		(C_TYPE_INT | 0x1008)
#define C_liloCmd		(C_TYPE_STR | 0x1009)
#define C_liloMap		(C_TYPE_STR | 0x100a)

#define C_GUIStyle		(C_TYPE_STR | 0x1011)
#define C_LogoArea		(C_TYPE_INT | 0x1012)	/* XXX to change */
# define LOGO_NONE	0
# define LOGO_LOGO	1
# define LOGO_CLOCK	2
#define C_LogoPixmap		(C_TYPE_STR | 0x1013)
#define C_GreeterPosFixed	(C_TYPE_INT | 0x1014)
#define C_GreeterPosX		(C_TYPE_INT | 0x1015)
#define C_GreeterPosY		(C_TYPE_INT | 0x1016)
#define C_StdFont		(C_TYPE_STR | 0x1017)
#define C_FailFont		(C_TYPE_STR | 0x1018)
#define C_GreetString		(C_TYPE_STR | 0x1019)
#define C_GreetFont		(C_TYPE_STR | 0x101a)
#define C_Language		(C_TYPE_STR | 0x101b)
#define C_ShowUsers		(C_TYPE_INT | 0x101c)
# define SHOW_ALL	0
# define SHOW_SEL	1
# define SHOW_NONE	2
#define C_SelectedUsers		(C_TYPE_ARGV | 0x101d)
#define C_HiddenUsers		(C_TYPE_ARGV | 0x101e)
#define C_MinShowUID		(C_TYPE_INT | 0x101f)
#define C_MaxShowUID		(C_TYPE_INT | 0x1020)
#define C_SortUsers		(C_TYPE_INT | 0x1021)
#define C_PreselectUser		(C_TYPE_INT | 0x1022)
# define PRESEL_NONE	0
# define PRESEL_PREV	1
# define PRESEL_DEFAULT	2
#define C_DefaultUser		(C_TYPE_STR | 0x1023)
#define C_FocusPasswd		(C_TYPE_INT | 0x1024)
#define C_EchoMode		(C_TYPE_INT | 0x1025)
# define ECHO_ONE	0	/* HACK! This must be equal to KPasswordEdit::EchoModes (kpassdlg.h) */
# define ECHO_THREE	1
# define ECHO_NONE	2
#define C_GreeterScreen		(C_TYPE_INT | 0x1026)
#define C_AntiAliasing		(C_TYPE_INT | 0x1027)
#define C_NumLock		(C_TYPE_INT | 0x1028)
#define C_UseBackground		(C_TYPE_INT | 0x1029)
#define C_BackgroundCfg		(C_TYPE_STR | 0x102a)
#define C_FaceSource		(C_TYPE_INT | 0x102b)
# define FACE_ADMIN_ONLY	0
# define FACE_PREFER_ADMIN	1
# define FACE_PREFER_USER	2
# define FACE_USER_ONLY		3
#define C_FaceDir		(C_TYPE_STR | 0x102c)
#define C_ColorScheme		(C_TYPE_STR | 0x102d)
#define C_ForgingSeed		(C_TYPE_INT | 0x102e)

#ifdef WITH_KDM_XCONSOLE
# define C_ShowLog		(C_TYPE_INT | 0x2000)
# define C_LogSource		(C_TYPE_STR | 0x2001)
#endif

#endif /* _KDM_CONFIG_H_ */
