    /*

    Greeter module for xdm
    $Id$

    Copyright (C) 1997, 1998 Steffen Hansen <hansen@kde.org>
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    */


#ifndef KGREETER_H
#define KGREETER_H

class QTimer;
class QLabel;
class QPushButton;
class QPopupMenu;
class QComboBox;

#include <qglobal.h>
#include <qlineedit.h>
#include <qframe.h>
#include <qmessagebox.h>

#include <kpassdlg.h>
#include <ksimpleconfig.h>
#include <klistview.h>

#include "kfdialog.h"
#include "kdmshutdown.h"
#ifdef BUILTIN_XCONSOLE
# include "kconsole.h"
#endif

class KdmClock;


class GreeterApp : public KApplication {
    typedef KApplication inherited;

public:
    GreeterApp( int& argc, char** argv );
    virtual bool x11EventFilter( XEvent * );

protected:
    virtual void timerEvent( QTimerEvent * );

private:
    static void sigAlarm( int );

    int pingInterval;
};


class KLoginLineEdit : public QLineEdit {
    Q_OBJECT
    typedef QLineEdit inherited;

public:
    KLoginLineEdit( QWidget *parent = 0 ) : inherited( parent ) { setMaxLength( 100 ); }

signals:
    void lost_focus();

protected:
    void focusOutEvent( QFocusEvent *e );
};


class KGreeter : public FDialog {
    Q_OBJECT
    typedef FDialog inherited;

public:
    KGreeter();
    ~KGreeter();
    void UpdateLock();
 
public slots:
    void accept();
    void reject();
    void chooser_button_clicked();
    void console_button_clicked();
    void quit_button_clicked();
    void shutdown_button_clicked();
    void slot_user_name( QListViewItem * );
    void slot_session_selected();
    void SetTimer();
    void timerDone();
    void sel_user();
    void load_wm();

protected:
    void timerEvent( QTimerEvent * ) {};
    void keyPressEvent( QKeyEvent * );
    void keyReleaseEvent( QKeyEvent * );

private:
    void set_wm( const char * );
    void insertUsers( KListView * );
    void insertUser( KListView *, const QImage &, const QString &, struct passwd * );
    void MsgBox( QMessageBox::Icon typ, QString msg ) { KFMsgBox::box( this, typ, msg ); }
    void Inserten( QPopupMenu *mnu, const QString& txt, const char *member );
    bool verifyUser( bool );
    void updateStatus();

    enum WmStat { WmNone, WmPrev, WmSel };
    WmStat		wmstat;
    QString		enam, user_pic_dir;
    KSimpleConfig	*stsfile;
    KListView		*user_view;
    KdmClock		*clock;
    QLabel		*pixLabel;
    QLabel		*loginLabel, *passwdLabel, *sessargLabel;
    KLoginLineEdit	*loginEdit;
    KPasswordEdit	*passwdEdit; 
    QComboBox		*sessargBox;
    QWidget		*sessargStat;
    QLabel		*sasPrev, *sasSel;
    QFrame		*separator;
    QTimer		*timer;
    QLabel		*failedLabel;
    QPushButton		*goButton, *clearButton;
    QPushButton		*menuButton;
    QPopupMenu		*optMenu;
    QPushButton		*quitButton;
    QPushButton		*shutdownButton;
    int			capslocked;
    bool		loginfailed;
#ifdef BUILTIN_XCONSOLE
    KConsole		*consoleView;
#endif
};


#endif /* KGREETER_H */

/*
 * Local variables:
 * mode: c++
 * c-file-style: "k&r"
 * End:
 */
