    /*

    Configuration for kdm. Class KDMConfig
    $Id$

    Copyright (C) 1997, 1998, 2000 Steffen Hansen <hansen@kde.org>
    Copyright (C) 2000-2002 Oswald Buddenhagen <ossi@kde.org>


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
 

#ifndef KDMCONFIG_H
#define KDMCONFIG_H

#include <qnamespace.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qfont.h>
#include <qpalette.h>

QString GetCfgQStr (int id);
QStringList GetCfgQStrList (int id);

class KDMConfig {

private:
    QFont Str2Font (const QString &aValue);
    QPalette Str2Palette (const QString &aValue);

public:
    KDMConfig();

    QFont	_normalFont;
    QFont	_failFont;
    QFont	_greetFont;

    int		_logoArea;
    QString	_logo;
    QString	_greetString;
    int		_greeterPosX, _greeterPosY;
    int		_greeterScreen;

    int		_showUsers;
    int		_preselUser;
    QString	_defaultUser;
    bool	_focusPasswd;
    bool	_sortUsers;
    QStringList	_users;
    QStringList	_noUsers;
    int		_lowUserId, _highUserId;
    int		_showRoot;
    int		_faceSource;
    int		_echoMode;
     
    QStringList	_sessionTypes;

    int		_allowShutdown, _allowNuke, _defSdMode;
    bool	_interactiveSd;

    int		_numLockStatus;

#if defined(__linux__) && defined(__i386)
    bool	_useLilo;
    QString	_liloCmd;
    QString	_liloMap;
#endif

    int		_loginMode;

#ifdef BUILTIN_XCONSOLE
    bool	_showLog;
    QString	_logSource;
#endif
};

extern KDMConfig *kdmcfg;

#endif /* KDMCONFIG_H */
