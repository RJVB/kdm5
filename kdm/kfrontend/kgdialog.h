    /*

    Base class for various kdm greeter dialogs

    Copyright (C) 1997, 1998 Steffen Hansen <hansen@kde.org>
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


#ifndef KGDIALOG_H
#define KGDIALOG_H

#include <config.h> // for WITH_KDM_XCONSOLE

#include "kfdialog.h"

class QPopupMenu;
class KConsole;
class QGridLayout;

#define ex_exit		1
#define ex_greet	2
#define ex_choose	3

class KGDialog : public FDialog {
    Q_OBJECT
    typedef FDialog inherited;

  public:
    KGDialog();

  public slots:
    void slotActivateMenu( int id );
    void slotExit();
    void slotSwitch();
    void slotReallySwitch();
    void slotConsole();
    void slotShutdown();

  protected:
#ifdef XDMCP
    void completeMenu( int _switchIf, int _switchCode, const QString &_switchMsg, int _switchAccel );
#else
    void completeMenu();
#endif
    void adjustGeometry();
    void inserten( const QString& txt, int accel, const char *member );
    void inserten( const QString& txt, int accel, QPopupMenu *cmnu );

    bool needSep;
    QPopupMenu *optMenu;
#ifdef WITH_KDM_XCONSOLE
    KConsole *consoleView;
    QGridLayout *layout;
#endif

  private:
    void ensureMenu();
    int switchCode;
};

#endif /* KGDIALOG_H */
