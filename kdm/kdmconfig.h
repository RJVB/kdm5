    /*

    Configuration for kdm. Class KDMConfig
    $Id$

    Copyright (C) 1997, 1998, 2000 Steffen Hansen
                                   hansen@kde.org


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
 

#ifndef KDMCONFIG_H
#define KDMCONFIG_H

#include "kdm-config.h"

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>	// for BSD
#endif
#include <unistd.h>

#include <qstring.h>
#include <qstrlist.h>
#include <qregexp.h>
#include <qfont.h>
#include <qcolor.h>
#include <qfile.h>
#include <qiconview.h>

#include <kconfig.h>

#include <qnamespace.h>

class KDMConfig {
public:
     KDMConfig();
     ~KDMConfig();
     
     QFont*          normalFont()      { return _normalFont;}
     QFont*          failFont()        { return _failFont;}
     QFont*          greetFont()       { return _greetFont;}
     QString         greetString()     { return _greetString;}
     QStringList     sessionTypes()    { return _sessionTypes;}
     int             shutdownButton()  { return _shutdownButton;}
     QString         shutdown()        { return _shutdown;}
     QString         restart()         { return _restart;}
     bool            useLogo()         { return _useLogo;}
     QString         logo()            { return _logo;}
     bool            users()           { return _show_users;}
     void           insertUsers( QIconView*, QStringList, bool);
     void           insertUsers( QIconView *i) {
	               insertUsers(i, _users, _sorted);
                    }
     // GUIStyle        style()           { return _style;}
	// None is defined as a macro somewhere in an X header. GRRRR.
     enum { KNone, All, RootOnly, ConsoleOnly };

     QString         liloCmd() { return _liloCmd; };
     QString         liloMap() { return _liloMap; };
     bool            useLilo() { return _useLilo; };
#ifndef BSD
     QString         consoleMode() { return _consoleMode; };
#endif

private:
     void           getConfig();
     KConfig*       kc;

     QFont*         _normalFont;
     QFont*         _failFont;
     QFont*         _greetFont;
     QString        _greetString;
     QStringList    _sessionTypes;
     int            _shutdownButton;
     QString        _shutdown;
     QString        _restart;
     QString        _logo;
     bool           _useLogo;
     QStringList    _users;
     bool           _show_users;
     bool           _sorted;
     // GUIStyle       _style;
     
     QString        _liloCmd;
     QString        _liloMap;
     bool           _useLilo;
#ifndef BSD
     QString        _consoleMode;
#endif
};

#endif /* KDMCONFIG_H */
