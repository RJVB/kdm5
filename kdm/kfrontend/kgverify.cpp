/*

Shell for kdm conversation plugins

Copyright (C) 1997, 1998, 2000 Steffen Hansen <hansen@kde.org>
Copyright (C) 2000-2004 Oswald Buddenhagen <ossi@kde.org>


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

#include <config.h>

#include "kgverify.h"
#include "kdmconfig.h"
#include "kdm_greet.h"

#include "themer/kdmthemer.h"
#include "themer/kdmitem.h"

#include <kapplication.h>
#include <klocale.h>
#include <klibloader.h>
#include <kseparator.h>
#include <kstdguiitem.h>
#include <kpushbutton.h>

#include <qregexp.h>
#include <qpopupmenu.h>
#include <qlayout.h>
#include <qfile.h>
#include <qlabel.h>

#include <pwd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h> // for updateLockStatus()
#include <fixx11h.h> // ... and make eventFilter() work again


void KGVerifyHandler::updateStatus( bool, bool )
{
}

KGVerify::KGVerify( KGVerifyHandler *_handler, KdmThemer *_themer,
                    QWidget *_parent, QWidget *_predecessor,
                    const QString &_fixedUser,
                    const PluginList &_pluginList,
                    KGreeterPlugin::Function _func,
                    KGreeterPlugin::Context _ctx )
	: inherited()
	, coreLock( 0 )
	, fixedEntity( _fixedUser )
	, pluginList( _pluginList )
	, handler( _handler )
	, themer( _themer )
	, parent( _parent )
	, predecessor( _predecessor )
	, plugMenu( 0 )
	, curPlugin( -1 )
	, func( _func )
	, ctx( _ctx )
	, enabled( true )
	, running( false )
	, suspended( false )
	, failed( false )
{
	connect( &timer, SIGNAL(timeout()), SLOT(slotTimeout()) );

	_parent->installEventFilter( this );
}

KGVerify::~KGVerify()
{
	Debug( "delete greet\n" );
	delete greet;
}

QPopupMenu *
KGVerify::getPlugMenu()
{
	// assert( !cont );
	if (!plugMenu) {
		uint np = pluginList.count();
		if (np > 1) {
			plugMenu = new QPopupMenu( parent );
			connect( plugMenu, SIGNAL(activated( int )),
			         SLOT(slotPluginSelected( int )) );
			for (uint i = 0; i < np; i++)
				plugMenu->insertItem( i18n(greetPlugins[pluginList[i]].info->name), pluginList[i] );
		}
	}
	return plugMenu;
}

bool // public
KGVerify::entitiesLocal() const
{
	return greetPlugins[pluginList[curPlugin]].info->flags & kgreeterplugin_info::Local;
}

bool // public
KGVerify::entitiesFielded() const
{
	return greetPlugins[pluginList[curPlugin]].info->flags & kgreeterplugin_info::Fielded;
}

QString // public
KGVerify::pluginName() const
{
	QString name( greetPlugins[pluginList[curPlugin]].library->fileName() );
	uint st = name.findRev( '/' ) + 1;
	uint en = name.find( '.', st );
	if (en - st > 7 && QConstString( name.unicode() + st, 7 ).string() == "kgreet_")
		st += 7;
	return name.mid( st, en - st );
}

static void
showWidgets( QLayoutItem *li )
{
	QWidget *w;
	QLayout *l;

	if ((w = li->widget()))
		w->show();
	else if ((l = li->layout())) {
		QLayoutIterator it = l->iterator();
		for (QLayoutItem *itm = it.current(); itm; itm = ++it)
			 showWidgets( itm );
	}
}

void // public
KGVerify::selectPlugin( int id )
{
	if (pluginList.isEmpty()) {
		MsgBox( errorbox, i18n("No greeter widget plugin loaded. Check the configuration.") );
		::exit( EX_UNMANAGE_DPY );
	}
	curPlugin = id;
	if (plugMenu)
		plugMenu->setItemChecked( id, true );
	greet = greetPlugins[pluginList[id]].info->create( this, themer, parent, predecessor, fixedEntity, func, ctx );
}

void // public
KGVerify::loadUsers( const QStringList &users )
{
	Debug( "greet->loadUsers(...)\n" );
	greet->loadUsers( users );
}

void // public
KGVerify::presetEntity( const QString &entity, int field )
{
	Debug( "greet->presetEntity(%\"s, %d)\n", entity.latin1(), field );
	greet->presetEntity( entity, field );
	timer.stop();
}

QString // public
KGVerify::getEntity() const
{
	Debug( "greet->getEntity()\n" );
	return greet->getEntity();
}

void
KGVerify::setUser( const QString &user )
{
	// assert( fixedEntity.isEmpty() );
	curUser = user;
	Debug( "greet->setUser(%\"s)\n", user.latin1() );
	greet->setUser( user );
	hasBegun = true;
	setTimer();
}

void
KGVerify::start()
{
	hasBegun = false;
	authTok = (func == KGreeterPlugin::ChAuthTok);
	running = true;
	Debug( "greet->start()\n" );
	greet->start();
	cont = false;
	if (!(func == KGreeterPlugin::Authenticate ||
	      ctx == KGreeterPlugin::ChangeTok ||
	      ctx == KGreeterPlugin::ExChangeTok))
	{
		cont = true;
		handleVerify();
	}
}

void
KGVerify::abort()
{
	Debug( "greet->abort()\n" );
	greet->abort();
	running = false;
}

void
KGVerify::suspend()
{
	// assert( !cont );
	if (running) {
		Debug( "greet->abort()\n" );
		greet->abort();
	}
	suspended = true;
	updateStatus();
}

void
KGVerify::resume()
{
	suspended = false;
	updateLockStatus();
	if (running) {
		Debug( "greet->start()\n" );
		greet->start();
	} else if (delayed) {
		delayed = false;
		running = true;
		Debug( "greet->start()\n" );
		greet->start();
		setTimer();
	}
}

void
KGVerify::accept()
{
	Debug( "greet->next()\n" );
	greet->next();
}

void
KGVerify::reject()
{
	// assert( !cont );
	curUser = QString::null;
	if (running) {
		Debug( "greet->abort()\n" );
		greet->abort();
	}
	Debug( "greet->clear()\n" );
	greet->clear();
	if (running) {
		Debug( "greet->start()\n" );
		greet->start();
	}
	hasBegun = false;
	if (!failed)
		timer.stop();
}

void
KGVerify::setEnabled( bool on )
{
	Debug( "greet->setEnabled(%s)\n", on ? "true" : "false" );
	greet->setEnabled( on );
	enabled = on;
	updateStatus();
}

void // private
KGVerify::slotTimeout()
{
	if (failed) {
		failed = false;
		updateStatus();
		Debug( "greet->revive()\n" );
		greet->revive();
		handler->verifyRetry();
		if (suspended)
			delayed = true;
		else {
			running = true;
			Debug( "greet->start()\n" );
			greet->start();
			setTimer();
			if (cont)
				handleVerify();
		}
	} else {
		// assert( ctx == Login );
		reject();
		handler->verifySetUser( QString::null );
	}
}

void // private
KGVerify::setTimer()
{
	if (func == KGreeterPlugin::Authenticate &&
	    ctx == KGreeterPlugin::Login &&
	    !failed && hasBegun)
		timer.start( 40000 );
}


void // private static
KGVerify::VMsgBox( QWidget *parent, const QString &user,
                   QMessageBox::Icon type, const QString &mesg )
{
	FDialog::box( parent, type, user.isEmpty() ?
	              mesg : i18n("Authenticating %1 ...\n\n").arg( user ) + mesg );
}

static const char *msgs[]= {
	I18N_NOOP( "You are required to change your password immediately (password aged)." ),
	I18N_NOOP( "You are required to change your password immediately (root enforced)." ),
	I18N_NOOP( "You are not allowed to login at the moment." ),
	I18N_NOOP( "Home folder not available." ),
	I18N_NOOP( "Logins are not allowed at the moment.\nTry again later." ),
	I18N_NOOP( "Your login shell is not listed in /etc/shells." ),
	I18N_NOOP( "Root logins are not allowed." ),
	I18N_NOOP( "Your account has expired; please contact your system administrator." )
};

void // private static
KGVerify::VErrBox( QWidget *parent, const QString &user, const char *msg )
{
	QMessageBox::Icon icon;
	QString mesg;

	if (!msg) {
		mesg = i18n("A critical error occurred.\n"
		            "Please look at KDM's logfile(s) for more information\n"
		            "or contact your system administrator.");
		icon = errorbox;
	} else {
		mesg = QString::fromLocal8Bit( msg );
		QString mesg1 = mesg + '.';
		for (uint i = 0; i < as(msgs); i++)
			if (mesg1 == msgs[i]) {
				mesg = i18n(msgs[i]);
				break;
			}
		icon = sorrybox;
	}
	VMsgBox( parent, user, icon, mesg );
}

void // private static
KGVerify::VInfoBox( QWidget *parent, const QString &user, const char *msg )
{
	QString mesg = QString::fromLocal8Bit( msg );
	QRegExp rx( "^Warning: your account will expire in (\\d+) day" );
	if (rx.search( mesg ) >= 0) {
		int expire = rx.cap( 1 ).toInt();
		mesg = expire ?
			i18n("Your account expires tomorrow.",
			     "Your account expires in %n days.", expire) :
			i18n("Your account expires today.");
	} else {
		rx.setPattern( "^Warning: your password will expire in (\\d+) day" );
		if (rx.search( mesg ) >= 0) {
			int expire = rx.cap( 1 ).toInt();
			mesg = expire ?
				i18n("Your password expires tomorrow.",
				     "Your password expires in %n days.", expire) :
				i18n("Your password expires today.");
		}
	}
	VMsgBox( parent, user, infobox, mesg );
}

bool // public static
KGVerify::handleFailVerify( QWidget *parent )
{
	Debug( "handleFailVerify ...\n" );
	char *msg = GRecvStr();
	QString user = QString::fromLocal8Bit( msg );
	free( msg );

	for (;;) {
		int ret = GRecvInt();

		// non-terminal status
		switch (ret) {
		/* case V_PUT_USER: cannot happen - we are in "classic" mode */
		/* case V_PRE_OK: cannot happen - not in ChTok dialog */
		/* case V_CHTOK: cannot happen - called by non-interactive verify */
		case V_CHTOK_AUTH:
			Debug( " V_CHTOK_AUTH\n" );
			{
				QStringList pgs( _pluginsLogin );
				pgs += _pluginsShutdown;
				QStringList::ConstIterator it;
				for (it = pgs.begin(); it != pgs.end(); ++it)
					if (*it == "classic" || *it == "modern") {
						pgs = *it;
						goto gotit;
					} else if (*it == "generic") {
						pgs = "modern";
						goto gotit;
					}
				pgs = "classic";
			  gotit:
				KGChTok chtok( parent, user, init( pgs ), 0,
				               KGreeterPlugin::AuthChAuthTok,
				               KGreeterPlugin::Login );
				return chtok.exec();
			}
		case V_MSG_ERR:
			Debug( " V_MSG_ERR\n" );
			msg = GRecvStr();
			Debug( "  message %\"s\n", msg );
			VErrBox( parent, user, msg );
			if (msg)
				free( msg );
			continue;
		case V_MSG_INFO:
			Debug( " V_MSG_INFO\n" );
			msg = GRecvStr();
			Debug( "  message %\"s\n", msg );
			VInfoBox( parent, user, msg );
			free( msg );
			continue;
		}

		// terminal status
		switch (ret) {
		case V_OK:
			Debug( " V_OK\n" );
			return true;
		case V_AUTH:
			Debug( " V_AUTH\n" );
			VMsgBox( parent, user, sorrybox, i18n("Authentication failed") );
			return false;
		case V_FAIL:
			Debug( " V_FAIL\n" );
			return false;
		default:
			LogPanic( "Unknown V_xxx code %d from core\n", ret );
		}
	}
}

void // private
KGVerify::handleVerify()
{
	QString user;

	Debug( "handleVerify ...\n" );
	for (;;) {
		char *msg;
		int ret, echo, ndelay;
		KGreeterPlugin::Function nfunc;

		ret = GRecvInt();

		// requests
		coreLock = 1;
		switch (ret) {
		case V_GET_TEXT:
			Debug( " V_GET_TEXT\n" );
			msg = GRecvStr();
			Debug( "  prompt %\"s\n", msg );
			echo = GRecvInt();
			Debug( "  echo = %d\n", echo );
			ndelay = GRecvInt();
			Debug( "  ndelay = %d\ngreet->textPrompt(...)\n", ndelay );
			greet->textPrompt( msg, echo, ndelay );
			if (msg)
				free( msg );
			return;
		case V_GET_BINARY:
			Debug( " V_GET_BINARY\n" );
			msg = GRecvArr( &ret );
			Debug( "  %d bytes prompt\n", ret );
			ndelay = GRecvInt();
			Debug( "  ndelay = %d\ngreet->binaryPrompt(...)\n", ndelay );
			greet->binaryPrompt( msg, ndelay );
			if (msg)
				free( msg );
			return;
		}

		// non-terminal status
		coreLock = 2;
		switch (ret) {
		case V_PUT_USER:
			Debug( " V_PUT_USER\n" );
			msg = GRecvStr();
			Debug( "  user %\"s\n", msg );
			curUser = user = QString::fromLocal8Bit( msg );
			// greet needs this to be able to return something useful from
			// getEntity(). but the backend is still unable to tell a domain ...
			Debug( "  greet->setUser()\n" );
			greet->setUser( curUser );
			handler->verifySetUser( curUser );
			if (msg)
				free( msg );
			continue;
		case V_PRE_OK: // this is only for func == AuthChAuthTok
			Debug( " V_PRE_OK\n" );
			// With the "classic" method, the wrong user simply cannot be
			// authenticated, even with the generic plugin. Other methods
			// could do so, but this applies only to ctx == ChangeTok, which
			// is not implemented yet.
			authTok = true;
			cont = true;
			Debug( "greet->succeeded()\n" );
			greet->succeeded();
			continue;
		case V_CHTOK_AUTH:
			Debug( " V_CHTOK_AUTH\n" );
			nfunc = KGreeterPlugin::AuthChAuthTok;
			user = curUser;
			goto dchtok;
		case V_CHTOK:
			Debug( " V_CHTOK\n" );
			nfunc = KGreeterPlugin::ChAuthTok;
			user = QString::null;
		  dchtok:
			{
				timer.stop();
				Debug( "greet->succeeded()\n" );
				greet->succeeded();
				KGChTok chtok( parent, user, pluginList, curPlugin, nfunc, KGreeterPlugin::Login );
				if (!chtok.exec())
					goto retry;
				handler->verifyOk();
				return;
			}
		case V_MSG_ERR:
			Debug( " V_MSG_ERR\n" );
			msg = GRecvStr();
			Debug( "  message %\"s\n", msg );
			if (!greet->textMessage( msg, true ))
				VErrBox( parent, user, msg );
			if (msg)
				free( msg );
			continue;
		case V_MSG_INFO:
			Debug( " V_MSG_INFO\n" );
			msg = GRecvStr();
			Debug( "  message %\"s\n", msg );
			if (!greet->textMessage( msg, false ))
				VInfoBox( parent, user, msg );
			free( msg );
			continue;
		}

		// terminal status
		coreLock = 0;
		running = false;
		timer.stop();

		if (ret == V_OK) {
			Debug( " V_OK\n" );
			if (!fixedEntity.isEmpty()) {
				QString ent = greet->getEntity();
				if (ent != fixedEntity) {
					Debug( "greet->failed()\n" );
					greet->failed();
					MsgBox( sorrybox,
					        i18n("Authenticated user (%1) does not match requested user (%2).\n")
					        .arg( ent ).arg( fixedEntity ) );
					goto retry;
				}
			}
			Debug( "greet->succeeded()\n" );
			greet->succeeded();
			handler->verifyOk();
			return;
		}

		Debug( "greet->failed()\n" );
		greet->failed();

		if (ret == V_AUTH) {
			Debug( " V_AUTH\n" );
			failed = true;
			updateStatus();
			handler->verifyFailed();
			timer.start( 1500 + kapp->random()/(RAND_MAX/1000) );
			return;
		}
		if (ret != V_FAIL)
			LogPanic( "Unknown V_xxx code %d from core\n", ret );
		Debug( " V_FAIL\n" );
	  retry:
		Debug( "greet->revive()\n" );
		greet->revive();
		running = true;
		Debug( "greet->start()\n" );
		greet->start();
		if (!cont)
			return;
		user = QString::null;
	}
}

void
KGVerify::gplugReturnText( const char *text, int tag )
{
	Debug( "gplugReturnText(%\"s, %d)\n",
	       tag & V_IS_SECRET ? "<masked>" : text, tag );
	GSendStr( text );
	if (text) {
		GSendInt( tag );
		hasBegun = true;
		setTimer();
		handleVerify();
	} else
		coreLock = 0;
}

void
KGVerify::gplugReturnBinary( const char *data )
{
	if (data) {
		unsigned const char *up = (unsigned const char *)data;
		int len = up[3] | (up[2] << 8) | (up[1] << 16) | (up[0] << 24);
		Debug( "gplugReturnBinary(%d bytes)\n", len );
		GSendArr( len, data );
		hasBegun = true;
		setTimer();
		handleVerify();
	} else {
		Debug( "gplugReturnBinary(NULL)\n" );
		GSendArr( 0, 0 );
		coreLock = 0;
	}
}

void
KGVerify::gplugSetUser( const QString &user )
{
	Debug( "gplugSetUser(%\"s)\n", user.latin1() );
	curUser = user;
	handler->verifySetUser( user );
	hasBegun = !user.isEmpty();
	setTimer();
}

void
KGVerify::gplugStart()
{
	// XXX handle func != Authenticate
	Debug( "gplugStart()\n" );
	GSendInt( ctx == KGreeterPlugin::Shutdown ? G_VerifyRootOK : G_Verify );
	GSendStr( greetPlugins[pluginList[curPlugin]].info->method );
	handleVerify();
}

void
KGVerify::gplugActivity()
{
	setTimer();
}

void
KGVerify::gplugMsgBox( QMessageBox::Icon type, const QString &text )
{
	MsgBox( type, text );
}

bool
KGVerify::eventFilter( QObject *o, QEvent *e )
{
	switch (e->type()) {
	case QEvent::KeyPress:
	case QEvent::KeyRelease:
		updateLockStatus();
	default:
		break;
	}
	return inherited::eventFilter( o, e );
}

void
KGVerify::updateLockStatus()
{
	unsigned int lmask;
	Window dummy1, dummy2;
	int dummy3, dummy4, dummy5, dummy6;
	XQueryPointer( qt_xdisplay(), DefaultRootWindow( qt_xdisplay() ),
	               &dummy1, &dummy2, &dummy3, &dummy4, &dummy5, &dummy6,
	               &lmask );
	capsLocked = lmask & LockMask;
	updateStatus();
}

void
KGVerify::MsgBox( QMessageBox::Icon typ, const QString &msg )
{
	timer.suspend();
	FDialog::box( parent, typ, msg );
	timer.resume();
}


QVariant // public static
KGVerify::getConf( void *, const char *key, const QVariant &dflt )
{
	if (!qstrcmp( key, "EchoMode" ))
		return QVariant( _echoMode );
	else {
		QString fkey = QString::fromLatin1( key ) + '=';
		for (QStringList::ConstIterator it = _pluginOptions.begin();
		     it != _pluginOptions.end(); ++it)
			if ((*it).startsWith( fkey ))
				return (*it).mid( fkey.length() );
		return dflt;
	}
}

QValueVector<GreeterPluginHandle> KGVerify::greetPlugins;

PluginList
KGVerify::init( const QStringList &plugins )
{
	PluginList pluginList;

	for (QStringList::ConstIterator it = plugins.begin(); it != plugins.end(); ++it) {
		GreeterPluginHandle plugin;
		QString path = KLibLoader::self()->findLibrary(
			((*it)[0] == '/' ? *it : "kgreet_" + *it ).latin1() );
		if (path.isEmpty()) {
			LogError( "GreeterPlugin %s does not exist\n", (*it).latin1() );
			continue;
		}
		uint i, np = greetPlugins.count();
		for (i = 0; i < np; i++)
			if (greetPlugins[i].library->fileName() == path)
				goto next;
		if (!(plugin.library = KLibLoader::self()->library( path.latin1() ))) {
			LogError( "Cannot load GreeterPlugin %s (%s)\n", (*it).latin1(), path.latin1() );
			continue;
		}
		if (!plugin.library->hasSymbol( "kgreeterplugin_info" )) {
			LogError( "GreeterPlugin %s (%s) is no valid greet widget plugin\n",
			          (*it).latin1(), path.latin1() );
			plugin.library->unload();
			continue;
		}
		plugin.info = (kgreeterplugin_info*)plugin.library->symbol( "kgreeterplugin_info" );
		if (!plugin.info->init( QString::null, getConf, 0 )) {
			LogError( "GreeterPlugin %s (%s) refuses to serve\n",
			          (*it).latin1(), path.latin1() );
			plugin.library->unload();
			continue;
		}
		Debug( "GreeterPlugin %s (%s) loaded\n", (*it).latin1(), plugin.info->name );
		greetPlugins.append( plugin );
	  next:
		pluginList.append( i );
	}
	return pluginList;
}

void
KGVerify::done()
{
	for (uint i = 0; i < greetPlugins.count(); i++) {
		if (greetPlugins[i].info->done)
			greetPlugins[i].info->done();
		greetPlugins[i].library->unload();
	}
}


KGStdVerify::KGStdVerify( KGVerifyHandler *_handler, QWidget *_parent,
                          QWidget *_predecessor, const QString &_fixedUser,
                          const PluginList &_pluginList,
                          KGreeterPlugin::Function _func,
                          KGreeterPlugin::Context _ctx )
	: inherited( _handler, 0, _parent, _predecessor, _fixedUser,
	             _pluginList, _func, _ctx )
	, failedLabelState( -1 )
{
	grid = new QGridLayout;
	grid->setAlignment( AlignCenter );

	failedLabel = new QLabel( parent );
	failedLabel->setFont( _failFont );
	grid->addWidget( failedLabel, 1, 0, AlignCenter );

	updateLockStatus();
}

KGStdVerify::~KGStdVerify()
{
	grid->removeItem( greet->getLayoutItem() );
}

void // public
KGStdVerify::selectPlugin( int id )
{
	inherited::selectPlugin( id );
	grid->addItem( greet->getLayoutItem(), 0, 0 );
	showWidgets( greet->getLayoutItem() );
}

void // private slot
KGStdVerify::slotPluginSelected( int id )
{
	if (failed)
		return;
	if (id != curPlugin) {
		plugMenu->setItemChecked( curPlugin, false );
		parent->setUpdatesEnabled( false );
		grid->removeItem( greet->getLayoutItem() );
		Debug( "delete greet\n" );
		delete greet;
		selectPlugin( id );
		handler->verifyPluginChanged( id );
		if (running)
			start();
		parent->setUpdatesEnabled( true );
	}
}

void
KGStdVerify::updateStatus()
{
	int nfls;

	if (!enabled)
		nfls = 0;
	else if (failed)
		nfls = 2;
	else if (!suspended && capsLocked)
		nfls = 1;
	else
		nfls = 0;

	if (failedLabelState != nfls) {
		failedLabelState = nfls;
		switch (nfls) {
		default:
			failedLabel->clear();
			break;
		case 1:
			failedLabel->setPaletteForegroundColor( Qt::red );
			failedLabel->setText( i18n("Warning: Caps Lock on") );
			break;
		case 2:
			failedLabel->setPaletteForegroundColor( Qt::black );
			failedLabel->setText( authTok ? i18n("Change failed") :
			                      fixedEntity.isEmpty() ?
			                      i18n("Login failed") : i18n("Authentication failed") );
			break;
		}
	}
}

KGThemedVerify::KGThemedVerify( KGVerifyHandler *_handler,
                                KdmThemer *_themer,
                                QWidget *_parent, QWidget *_predecessor,
                                const QString &_fixedUser,
                                const PluginList &_pluginList,
                                KGreeterPlugin::Function _func,
                                KGreeterPlugin::Context _ctx )
	: inherited( _handler, _themer, _parent, _predecessor, _fixedUser,
	             _pluginList, _func, _ctx )
{
	updateLockStatus();
}

KGThemedVerify::~KGThemedVerify()
{
}

void // public
KGThemedVerify::selectPlugin( int id )
{
	inherited::selectPlugin( id );
	QLayoutItem *l;
	KdmItem *n;
	if (themer && (l = greet->getLayoutItem())) {
		if (!(n = themer->findNode( "talker" )))
			MsgBox( errorbox,
			        i18n("Theme not usable with authentication method '%1'.")
			        .arg( i18n(greetPlugins[pluginList[id]].info->name) ) );
		else {
			n->setLayoutItem( l );
			showWidgets( l );
		}
	}
	if (themer)
		themer->updateGeometry( true );
}

void // private slot
KGThemedVerify::slotPluginSelected( int id )
{
	if (failed)
		return;
	if (id != curPlugin) {
		plugMenu->setItemChecked( curPlugin, false );
		Debug( "delete greet\n" );
		delete greet;
		selectPlugin( id );
		handler->verifyPluginChanged( id );
		if (running)
			start();
	}
}

void
KGThemedVerify::updateStatus()
{
	handler->updateStatus( enabled && failed,
	                       enabled && !suspended && capsLocked );
}


KGChTok::KGChTok( QWidget *_parent, const QString &user,
                  const PluginList &pluginList, int curPlugin,
                  KGreeterPlugin::Function func,
                  KGreeterPlugin::Context ctx )
	: inherited( _parent )
	, verify( 0 )
{
	QSizePolicy fp( QSizePolicy::Fixed, QSizePolicy::Fixed );
	okButton = new KPushButton( KStdGuiItem::ok(), this );
	okButton->setSizePolicy( fp );
	okButton->setDefault( true );
	cancelButton = new KPushButton( KStdGuiItem::cancel(), this );
	cancelButton->setSizePolicy( fp );

	verify = new KGStdVerify( this, this, cancelButton, user, pluginList, func, ctx );
	verify->selectPlugin( curPlugin );

	QVBoxLayout *box = new QVBoxLayout( this, 10 );

	box->addWidget( new QLabel( i18n("Changing authentication token"), this ), 0, AlignHCenter );

	box->addLayout( verify->getLayout() );

	box->addWidget( new KSeparator( KSeparator::HLine, this ) );

	QHBoxLayout *hlay = new QHBoxLayout( box );
	hlay->addStretch( 1 );
	hlay->addWidget( okButton );
	hlay->addStretch( 1 );
	hlay->addWidget( cancelButton );
	hlay->addStretch( 1 );

	connect( okButton, SIGNAL(clicked()), SLOT(accept()) );
	connect( cancelButton, SIGNAL(clicked()), SLOT(reject()) );

	QTimer::singleShot( 0, verify, SLOT(start()) );
}

KGChTok::~KGChTok()
{
	hide();
	delete verify;
}

void
KGChTok::accept()
{
	verify->accept();
}

void
KGChTok::verifyPluginChanged( int )
{
	// cannot happen
}

void
KGChTok::verifyOk()
{
	inherited::accept();
}

void
KGChTok::verifyFailed()
{
	okButton->setEnabled( false );
	cancelButton->setEnabled( false );
}

void
KGChTok::verifyRetry()
{
	okButton->setEnabled( true );
	cancelButton->setEnabled( true );
}

void
KGChTok::verifySetUser( const QString & )
{
	// cannot happen
}


////// helper class, nuke when qtimer supports suspend()/resume()

QXTimer::QXTimer()
	: inherited( 0 )
	, left( -1 )
{
	connect( &timer, SIGNAL(timeout()), SLOT(slotTimeout()) );
}

void
QXTimer::start( int msec )
{
	left = msec;
	timer.start( left, true );
	gettimeofday( &stv, 0 );
}

void
QXTimer::stop()
{
	timer.stop();
	left = -1;
}

void
QXTimer::suspend()
{
	if (timer.isActive()) {
		timer.stop();
		struct timeval tv;
		gettimeofday( &tv, 0 );
		left -= (tv.tv_sec - stv.tv_sec) * 1000 + (tv.tv_usec - stv.tv_usec) / 1000;
		if (left < 0)
			left = 0;
	}
}

void
QXTimer::resume()
{
	if (left >= 0 && !timer.isActive()) {
		timer.start( left, true );
		gettimeofday( &stv, 0 );
	}
}

void
QXTimer::slotTimeout()
{
	left = 0;
	emit timeout();
}


#include "kgverify.moc"
