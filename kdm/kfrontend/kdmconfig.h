/*

Configuration for kdm

Copyright (C) 1997, 1998, 2000 Steffen Hansen <hansen@kde.org>
Copyright (C) 2000-2003 Oswald Buddenhagen <ossi@kde.org>


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

#include <config.h>

#include "config.ci"

#ifdef __cplusplus

#include <qstring.h>
#include <qstringlist.h>
#include <qfont.h>

extern QString _stsFile;
extern bool _isLocal;
extern bool _authorized;

CONF_GREET_CPP_DECLS

extern "C"
#endif
void init_config( void );

CONF_GREET_C_DECLS

#endif /* KDMCONFIG_H */
