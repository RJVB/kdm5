/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 1999 Martin R. Jones <mjones@kde.org>
Copyright (C) 2003 Oswald Buddenhagen <ossi@kde.org>
Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef SCREENLOCKER_UNLOCKER_H
#define SCREENLOCKER_UNLOCKER_H

#include <QtCore/QAbstractListModel>
#include <QtDeclarative/QDeclarativeItem>
#include <kgreeterplugin.h>

// forward declarations
class KGreetPlugin;
class KLibrary;
class QSocketNotifier;

struct GreeterPluginHandle {
    KLibrary *library;
    KGreeterPluginInfo *info;
};

namespace ScreenLocker
{
class Unlocker;

class UnlockerItem : public QDeclarativeItem
{
    Q_OBJECT
public:
    UnlockerItem(QDeclarativeItem *parent = NULL);
    virtual ~UnlockerItem();

    QGraphicsProxyWidget *proxy();

public Q_SLOTS:
    void verify();

Q_SIGNALS:
    void greeterFailed();
    void greeterReady();
    void greeterMessage(const QString &text);
    void greeterAccepted();
private:
    void init();
    QGraphicsProxyWidget *m_proxy;
    Unlocker *m_unlocker;
};

class KeyboardItem : public QDeclarativeItem
{
    Q_OBJECT
public:
    KeyboardItem(QDeclarativeItem *parent = NULL);
    virtual ~KeyboardItem();

private:
    QWidget *m_widget;
    QGraphicsProxyWidget *m_proxy;
};

class UserSessionsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    UserSessionsModel(QObject *parent = 0);
    virtual ~UserSessionsModel();

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;

private:
    class UserSessionItem;
    void init();
    QList<UserSessionItem> m_model;
};

class UserSessionsModel::UserSessionItem
{
public:
    UserSessionItem(const QString &session, const QString &location, int vt, bool enabled)
        : m_session(session)
        , m_location(location)
        , m_vt(vt)
        , m_enabled(enabled)
    {}
    QString m_session;
    QString m_location;
    int m_vt;
    bool m_enabled;
};

/**
 * @short Class which checks authentication through KGreeterPlugin framework.
 *
 * This class can be used to perform an authentication through the KGreeterPlugin framework.
 * It provides a QWidget containing the widgets provided by the various greeter.
 *
 * To perform an authentication through the greeter invoke the @link verify slot. The class
 * will emit either @link greeterAccepted or @link greeterFailed signals depending on whether
 * the authentication succeeded or failed.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 **/
class Unlocker : public QObject, public KGreeterPluginHandler
{
    Q_OBJECT
public:
    Unlocker(QObject *parent);
    virtual ~Unlocker();
    bool isValid() const {
        return m_valid;
    }
    // from KGreetPluginHandler
    virtual void gplugReturnText(const char *text, int tag);
    virtual void gplugReturnBinary(const char *data);
    virtual void gplugSetUser(const QString &);
    virtual void gplugStart();
    virtual void gplugChanged();
    virtual void gplugActivity();
    virtual void gplugMsgBox(QMessageBox::Icon type, const QString &text);
    virtual bool gplugHasNode(const QString &id);

    QWidget *greeterWidget() const {
        return m_greeterWidget;
    }

    QAbstractItemModel *sessionModel() const {
        return m_sessionModel;
    }

    /**
     * @returns @c true if switching between user sessions is possible, @c false otherwise.
     **/
    bool isSwitchUserSupported() const;
    /**
     * @returns @c true if a new session can be started, @c false otherwise.
     **/
    bool isStartNewSessionSupported() const;

Q_SIGNALS:
    /**
     * Signal emitted in case the authentication through the greeter succeeded.
     **/
    void greeterAccepted();
    /**
     * Signal emitted in case the authentication through the greeter failed.
     */
    void greeterFailed();
    /**
     * Signal emitted when the greeter is ready to perform authentication.
     * E.g. after the timeout of a failed authentication attempt.
     **/
    void greeterReady();
    /**
     * Signal broadcasting any messages from the greeter.
     **/
    void greeterMessage(const QString &text);

public Q_SLOTS:
    /**
     * Invoke to perform an authentication through the greeter plugins.
     **/
    void verify();
    /**
     * Invoke to start a new session if allowed.
     **/
    void startNewSession();
    void activateSession(int index);

private Q_SLOTS:
    void handleVerify();
    void failedTimer();
private:
    void initialize();
    bool loadGreetPlugin();
    static QVariant getConf(void *ctx, const char *key, const QVariant &dflt);

    // kcheckpass interface
    int Reader(void *buf, int count);
    bool GRead(void *buf, int count);
    bool GWrite(const void *buf, int count);
    bool GSendInt(int val);
    bool GSendStr(const char *buf);
    bool GSendArr(int len, const char *buf);
    bool GRecvInt(int *val);
    bool GRecvArr(char **buf);
    void reapVerify();
    void cantCheck();

    GreeterPluginHandle m_pluginHandle;
    QWidget *m_greeterWidget;
    KGreeterPlugin *m_greet;
    QStringList m_plugins;
    QStringList m_pluginOptions;
    QString m_method;
    bool m_valid;
    // for kcheckpass
    int  m_pid;
    int m_fd;
    QSocketNotifier *m_notifier;
    UserSessionsModel *m_sessionModel;
    bool m_failedLock;
};

} // end namespace
#endif
