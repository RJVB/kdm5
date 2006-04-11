/* vi: ts=8 sts=4 sw=4
 *
 * This file is part of the KDE project, module kdesktop.
 * Copyright (C) 1999 Geert Jansen <g.t.jansen@stud.tue.nl>
 * 
 * You can Freely distribute this program under the GNU General Public
 * License. See the file "COPYING" for the exact licensing terms.
 *
 *
 * Shared pixmap server for KDE.
 *
 * 5 Dec 99: Geert Jansen:
 *
 *	Initial implementation using the X11 selection mechanism.
 */

#include <assert.h>


#include <kapplication.h>
#include <kdebug.h>

#include <X11/X.h>
#include <X11/Xlib.h>

#include "pixmapserver.h"
//Added by qt3to4:
#include <QPixmap>
#include <QX11Info>

#ifndef None
#define None 0L
#endif

#ifdef __GNUC__
#define ID __PRETTY_FUNCTION__ << ": "
#else
#define ID "KPixmapServer: "
#endif


KPixmapServer::KPixmapServer()
    : QWidget(0L, "shpixmap comm window")
{
    kapp->installX11EventFilter(this);
    pixmap = XInternAtom(QX11Info::display(), "PIXMAP", false);
}


KPixmapServer::~KPixmapServer()
{
    SelectionIterator it;
    for (it=m_Selections.begin(); it!=m_Selections.end(); it++)
	XSetSelectionOwner(QX11Info::display(), it.key(), None, CurrentTime);

    DataIterator it2;
    for (it2=m_Data.begin(); it2!=m_Data.end(); it2++)
	delete it2.value().pixmap;
}


void KPixmapServer::add(QString name, QPixmap *pm, bool overwrite)
{
    if (m_Names.contains(name)) 
    {
	if (overwrite)
	    remove(name);
	else return;
    }
	
    QString str = QString("KDESHPIXMAP:%1").arg(name);
    Atom sel = XInternAtom(QX11Info::display(), str.latin1(), false);
    KPixmapInode pi;
    pi.handle = pm->handle();
    pi.selection = sel;
    m_Names[name] = pi;

    KSelectionInode si;
    si.name = name;
    si.handle = pm->handle();
    m_Selections[sel] = si;

    DataIterator it = m_Data.find(pm->handle());
    if (it == m_Data.end()) 
    {
	KPixmapData data;
	data.pixmap = pm;
	data.usecount = 0;
	data.refcount = 1;
	m_Data[pm->handle()] = data;
    } else
	it.value().refcount++;

    XSetSelectionOwner(QX11Info::display(), sel, winId(), CurrentTime);
}


void KPixmapServer::remove(QString name)
{
    // Remove the name
    NameIterator it = m_Names.find(name);
    if (it == m_Names.end())
	return;
    KPixmapInode pi = it.value();
    m_Names.erase(it);

    // Remove and disown the selection
    SelectionIterator it2 = m_Selections.find(pi.selection);
    assert(it2 != m_Selections.end());
    m_Selections.erase(it2);
    XSetSelectionOwner(QX11Info::display(), pi.selection, None, CurrentTime);

    // Decrease refcount on data
    DataIterator it3 = m_Data.find(pi.handle);
    assert(it3 != m_Data.end());
    it3.value().refcount--;
    if (!it3.value().refcount && !it3.value().usecount) 
    {
	delete it3.value().pixmap;
	m_Data.erase(it3);
    }
}


QStringList KPixmapServer::list()
{
    QStringList lst;
    NameIterator it;
    for (it=m_Names.begin(); it!=m_Names.end(); it++)
	lst += it.key();
    return lst;
}


void KPixmapServer::setOwner(QString name)
{
    NameIterator it = m_Names.find(name);
    if (it == m_Names.end())
	return;

    XSetSelectionOwner(QX11Info::display(), it.value().selection, winId(), CurrentTime);
}


bool KPixmapServer::x11Event(XEvent *event)
{
    // Handle SelectionRequest events by which a X client can request a
    // shared pixmap.

    if (event->type == SelectionRequest) 
    {
	XSelectionRequestEvent *ev = &event->xselectionrequest;

	// Build negative reply
	XEvent reply;
	reply.type = SelectionNotify;
	reply.xselection.display = QX11Info::display();
	reply.xselection.requestor = ev->requestor;
	reply.xselection.selection = ev->selection;
	reply.xselection.target = pixmap;
	reply.xselection.property = None;
	reply.xselection.time = ev->time;

	// Check if we know about this selection
	Atom sel = ev->selection;
	SelectionIterator it = m_Selections.find(sel);
	if (it == m_Selections.end())
	    return false;
	KSelectionInode si = it.value();

	// Only convert to pixmap
	if (ev->target != pixmap) 
	{
	    kDebug(1204) << ID << "illegal target\n";
	    XSendEvent(QX11Info::display(), ev->requestor, false, 0, &reply);
	    return true;
	}

	// Check if there is no transaction in progress to the same property
	if (m_Active.contains(ev->property)) 
	{
	    kDebug(1204) << ID << "selection is busy.\n";
	    XSendEvent(QX11Info::display(), ev->requestor, false, 0, &reply);
	    return true;
	}

	// Check if the selection was not deleted
	DataIterator it2 = m_Data.find(si.handle);
	if (it2 == m_Data.end()) 
	{
	    kDebug(1204) << ID << "selection has been deleted.\n";
	    XSendEvent(QX11Info::display(), ev->requestor, false, 0, &reply);
	    return true;
	}

	kDebug(1204) << ID << "request for " << si.name << "\n";

	// All OK: pass the pixmap handle.
	XChangeProperty(QX11Info::display(), ev->requestor, ev->property, pixmap,
		32, PropModeReplace, (unsigned char *) &si.handle, 1);
	it2.value().usecount++;
	m_Active[ev->property] = si.handle;

	// Request PropertyNotify events for the target window
	// XXX: The target window better not be handled by us!
	XSelectInput(QX11Info::display(), ev->requestor, PropertyChangeMask);

	// Acknowledge to the client and return
	reply.xselection.property = ev->property;
	XSendEvent(QX11Info::display(), ev->requestor, false, 0, &reply);
	return true;
    }

    // ICCCM says that the target property is to be deleted by the
    // requestor. We are notified of this by a PropertyNotify. Only then, we
    // can actually delete the pixmap if it was removed.

    if (event->type == PropertyNotify) 
    {
	XPropertyEvent *ev = &event->xproperty;

	AtomIterator it = m_Active.find(ev->atom);
	if (it == m_Active.end())
	    return false;
	Qt::HANDLE handle = it.value();
	m_Active.erase(it);

	DataIterator it2 = m_Data.find(handle);
	assert(it2 != m_Data.end());
	it2.value().usecount--;
	if (!it2.value().usecount && !it2.value().refcount) 
	{
	    delete it2.value().pixmap;
	    m_Data.erase(it2);
	}
	return true;
    }
        
    // Handle SelectionClear events.

    if (event->type == SelectionClear) 
    {
	XSelectionClearEvent *ev = &event->xselectionclear;

	SelectionIterator it = m_Selections.find(ev->selection);
	if (it == m_Selections.end())
	    return false;

	emit selectionCleared(it.value().name);
	return  true;
    }

    // Process further
    return false;
}

#include "pixmapserver.moc"
