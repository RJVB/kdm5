/*
 * main.cpp
 *
 * Copyright (c) 1999 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
 *
 * Requires the Qt widget libraries, available at no cost at
 * http://www.troll.no/
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "main.h"

#include <config-workspace.h>
#include <config-kdm.h>

#include "background.h"
#include "kdm-gen.h"
#include "kdm-dlg.h"
#include "kdm-users.h"
#include "kdm-shut.h"
#include "kdm-conv.h"
#include "kdm-theme.h"

#include "helper.h"

#include <kaboutdata.h>
#include <klocalizedstring.h>
#include <kmessagebox.h>
#include <kconfig.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>

#include <QDebug>
#include <QTemporaryFile>
#include <QFile>
#include <QMimeData>
#include <QMimeType>
#include <QMimeDatabase>
#include <QImageReader>
#include <QDropEvent>
#include <QLabel>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QGuiApplication>

#include <sys/types.h>
#include <unistd.h>
#include <locale.h>
#include <pwd.h>
#include <grp.h>

static QByteArray origPluginPath;

K_PLUGIN_FACTORY(KDMFactory, if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        registerPlugin<KDModule>();
        origPluginPath = qgetenv("QT_PLUGIN_PATH");
        // KDM will (likely) run without QT_PLUGIN_PATH set, so make certain we do, too.
        qunsetenv("QT_PLUGIN_PATH");
    } else {
        qCritical() << "KDM Control module not supported";
    }; )

int handleKauthActionJob(QWidget *parent, KAuth::ExecuteJob *j, QVariantMap *returnedData = 0)
{
    int code = 0;
    // for now until we figure out how to get the ActionReply
    Q_UNUSED(returnedData);

    if (!j->exec()) {
        if (j->error() == KAuth::ActionReply::AuthorizationDeniedError) {
            KMessageBox::error(parent, i18n("Permission denied."), i18n("KDM Control Module"));
        } else {
            code = j->error();
            KMessageBox::error(parent, i18n("Error while authenticating action:\n%1 (code %2)", j->errorString(), code),
                               i18n("KDM Control Module"));
        }
    }

    return code;
}

QUrl *decodeImgDrop(QDropEvent *e, QWidget *wdg)
{
    auto uriList = e->mimeData()->urls();
    if (!uriList.isEmpty()) {
        QUrl *url = new QUrl(uriList.first());

        const auto mime = QMimeDatabase().mimeTypeForUrl(*url);
        if (QImageReader::supportedMimeTypes().contains(mime.name().toLatin1())) {
            return url;
        }

        auto qs = QImageReader::supportedImageFormats();
        qs.removeFirst();

        QString msg = i18n(
            "%1 does not appear to be an image file.\n"
            "Please use files with these extensions:\n"
            "%2",
            url->fileName(), QString(qs.join("\n")));
        KMessageBox::sorry(wdg, msg);
        delete url;
    }
    return 0;
}

KConfig *config;

KDModule::KDModule(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , minshowuid(0)
    , maxshowuid(0)
    , updateOK(false)
{
    KAboutData *about =
        new KAboutData(QStringLiteral("kcmkdm"), QStringLiteral("kdmconfig"),
                       QStringLiteral(KDM5_VERSION), i18n("KDE Login Manager Config Module"),
                       KAboutLicense::GPL,
                       i18n("(c) 1996-2010 The KDM Authors"), QString(),
                       QStringLiteral("http://developer.kde.org/~ossi/sw/kdm.html"));

    about->addAuthor(i18n("Thomas Tanghus"), i18n("Original author"), "tanghus@earthling.net");
    about->addAuthor(i18n("Steffen Hansen"), QString(), "hansen@kde.org");
    about->addAuthor(i18n("Oswald Buddenhagen"), i18n("Previous maintainer"), "ossi@kde.org");
    about->addAuthor(i18n("Stephen Leaf"), QString(), "smileaf@smileaf.org");
    about->addAuthor(i18n("Igor Krivenko"), QString(), "igor@shg.ru");
    about->addAuthor(i18n("R.J.V. Bertin"), i18n("Porting to Qt5/KF5"), "rjvbertin@gmail.com");

    setQuickHelp(i18n(
        "<h1>Login Manager</h1> In this module you can configure the "
        "various aspects of the KDE Login Manager. This includes "
        "the look and feel as well as the users that can be "
        "selected for login. Note that you can only make changes "
        "if you run the module with superuser rights."
        "<h2>General</h2> On this tab page, you can configure parts of "
        "the Login Manager's look, and which language it should use. "
        "The language settings made here have no influence on "
        "the user's language settings."
        "<h2>Dialog</h2>Here you can configure the look of the \"classical\" "
        "dialog based mode if you have chosen to use it. "
        "<h2>Background</h2>If you want to set a special background for the dialog-based "
        "login screen, this is where to do it."
        "<h2>Themes</h2> Here you can specify the theme to be used by the Login Manager."
        "<h2>Shutdown</h2> Here you can specify who is allowed to shutdown/reboot the machine "
        "and whether a boot manager should be used."
        "<h2>Users</h2>On this tab page, you can select which users the Login Manager "
        "will offer you for logging in."
        "<h2>Convenience</h2> Here you can specify a user to be logged in automatically, "
        "users not needing to provide a password to log in, and other convenience features.<br/>"
        "Note that by their nature, these settings are security holes, so use them very carefully."));

    setAboutData(about);
    setNeedsAuthorization(true);

    setlocale(LC_COLLATE, "C");

    KLocalizedString::setApplicationDomain("kcmbackground");

    QStringList sl;
    QMap<gid_t, QStringList> tgmap;
    QMap<gid_t, QStringList>::Iterator tgmapi;
    QMap<gid_t, QStringList>::ConstIterator tgmapci;
    QMap<QString, QPair<int, QStringList> >::Iterator umapi;

    struct passwd *ps;
    for (setpwent(); (ps = getpwent());) {
        QString un(QFile::decodeName(ps->pw_name));
        if (usermap.find(un) == usermap.end()) {
            usermap.insert(un, QPair<int, QStringList>(ps->pw_uid, sl));
            if ((tgmapi = tgmap.find(ps->pw_gid)) != tgmap.end())
                (*tgmapi).append(un);
            else
                tgmap[ps->pw_gid] = QStringList(un);
        }
    }
    endpwent();

    struct group *grp;
    for (setgrent(); (grp = getgrent());) {
        QString gn(QFile::decodeName(grp->gr_name));
        bool delme = false;
        if ((tgmapi = tgmap.find(grp->gr_gid)) != tgmap.end()) {
            if ((*tgmapi).count() == 1 && (*tgmapi).first() == gn)
                delme = true;
            else
                for (QStringList::ConstIterator it = (*tgmapi).constBegin();
                        it != (*tgmapi).constEnd(); ++it)
                    usermap[*it].second.append(gn);
            tgmap.erase(tgmapi);
        }
        if (!*grp->gr_mem ||
                (delme && !grp->gr_mem[1] && gn == QFile::decodeName(*grp->gr_mem)))
            continue;
        do {
            QString un(QFile::decodeName(*grp->gr_mem));
            if ((umapi = usermap.find(un)) != usermap.end()) {
                if (!(*umapi).second.contains(gn))
                    (*umapi).second.append(gn);
            } else
                qWarning() << "group '" << gn << "' contains unknown user '" << un << "'" ;
        } while (*++grp->gr_mem);
    }
    endgrent();

    for (tgmapci = tgmap.constBegin(); tgmapci != tgmap.constEnd(); ++tgmapci)
        qWarning() << "user(s) '" << tgmapci.value().join(",")
                   << "' have unknown GID " << tgmapci.key() << endl;

    config = createTempConfig();

    QVBoxLayout *top = new QVBoxLayout(this);
    top->setMargin(0);
    tab = new QTabWidget(this);

    general = new KDMGeneralWidget(this);
    tab->addTab(general, i18n("&General"));
    connect(general, SIGNAL(changed()), SLOT(changed()));
    connect(general, SIGNAL(useThemeChanged(bool)),
            SLOT(slotUseThemeChanged(bool)));

    dialog_stack = new QStackedWidget(this);
    tab->addTab(dialog_stack, i18n("&Dialog"));
    dialog = new KDMDialogWidget(dialog_stack);
    dialog_stack->addWidget(dialog);
    connect(dialog, SIGNAL(changed()), SLOT(changed()));
    QLabel *lbl = new QLabel(
        i18n("There is no login dialog window in themed mode."),
        dialog_stack);
    lbl->setAlignment(Qt::AlignCenter);
    dialog_stack->addWidget(lbl);

    background_stack = new QStackedWidget(this);
    tab->addTab(background_stack, i18n("&Background"));
    background = new KBackground(createBackgroundTempConfig(), background_stack);
    background_stack->addWidget(background);
    connect(background, SIGNAL(changed()), SLOT(changed()));
    lbl = new QLabel(
        i18n("The background cannot be configured separately in themed mode."),
        background_stack);
    lbl->setAlignment(Qt::AlignCenter);
    background_stack->addWidget(lbl);

#ifdef KDM_THEMEABLE
    theme_stack = new QStackedWidget(this);
    tab->addTab(theme_stack, i18n("&Theme"));
    lbl = new QLabel(
        i18n("Themed mode is disabled. See \"General\" tab."),
        theme_stack);
    lbl->setAlignment(Qt::AlignCenter);
    theme_stack->addWidget(lbl);
    theme = new KDMThemeWidget(theme_stack);
    theme_stack->addWidget(theme);
    connect(theme, SIGNAL(changed()), SLOT(changed()));
#endif

    sessions = new KDMSessionsWidget(this);
    tab->addTab(sessions, i18n("&Shutdown"));
    connect(sessions, SIGNAL(changed()), SLOT(changed()));

    users = new KDMUsersWidget(this);
    tab->addTab(users, i18n("&Users"));
    connect(users, SIGNAL(changed()), SLOT(changed()));
    connect(users, SIGNAL(setMinMaxUID(int,int)), SLOT(slotMinMaxUID(int,int)));
    connect(this, SIGNAL(addUsers(QMap<QString,int>)),
            users, SLOT(slotAddUsers(QMap<QString,int>)));
    connect(this, SIGNAL(delUsers(QMap<QString,int>)),
            users, SLOT(slotDelUsers(QMap<QString,int>)));
    connect(this, SIGNAL(clearUsers()), users, SLOT(slotClearUsers()));

    convenience = new KDMConvenienceWidget(this);
    tab->addTab(convenience, i18n("&Convenience"));
    connect(convenience, SIGNAL(changed()), SLOT(changed()));
    connect(this, SIGNAL(addUsers(QMap<QString,int>)),
            convenience, SLOT(slotAddUsers(QMap<QString,int>)));
    connect(this, SIGNAL(delUsers(QMap<QString,int>)),
            convenience, SLOT(slotDelUsers(QMap<QString,int>)));
    connect(this, SIGNAL(clearUsers()), convenience, SLOT(slotClearUsers()));

    top->addWidget(tab);

    setNeedsAuthorization(true);
}

KConfig *KDModule::createTempConfig()
{
    pTempConfigFile = new QTemporaryFile;
    pTempConfigFile->open();
    QString tempConfigName = pTempConfigFile->fileName();

    KConfig *pSystemKDMConfig = new KConfig(
        QString::fromLatin1(KDE_CONFDIR "/kdm/kdmrc"), KConfig::SimpleConfig);

    KConfig *pTempConfig = pSystemKDMConfig->copyTo(tempConfigName);
    pTempConfig->sync();

    QFile::setPermissions(tempConfigName,
                          pTempConfigFile->permissions() | QFile::ReadOther);

    return pTempConfig;
}

KSharedConfigPtr KDModule::createBackgroundTempConfig()
{
    pBackgroundTempConfigFile = new QTemporaryFile;
    pBackgroundTempConfigFile->open();
    QString tempBackgroundConfigName = pBackgroundTempConfigFile->fileName();

    QString systemBackgroundConfigName =
        config->group("X-*-Greeter").readEntry(
            "BackgroundCfg", KDE_CONFDIR "/kdm/backgroundrc");

    KConfig systemBackgroundConfig(systemBackgroundConfigName);
    KSharedConfigPtr pTempConfig = KSharedConfig::openConfig(tempBackgroundConfigName);
    systemBackgroundConfig.copyTo(tempBackgroundConfigName, pTempConfig.data());
    pTempConfig->sync();

    QFile::setPermissions(tempBackgroundConfigName,
                          pBackgroundTempConfigFile->permissions() | QFile::ReadOther);

    return pTempConfig;
}

KDModule::~KDModule()
{
    delete config;
    delete pBackgroundTempConfigFile;
    delete pTempConfigFile;
    if (!origPluginPath.isEmpty()) {
        qputenv("QT_PLUGIN_PATH", origPluginPath);
    }
}

void KDModule::load()
{
    general->load();
    dialog->load();
    background->load();
#ifdef KDM_THEMEABLE
    theme->load();
#endif
    users->load();
    sessions->load();
    convenience->load();
    propagateUsers();

    emit changed(false);
}


void KDModule::save()
{
    general->save();
    dialog->save();
    background->save();
#ifdef KDM_THEMEABLE
    theme->save();
#endif
    users->save();
    sessions->save();
    convenience->save();
    config->sync();

    QVariantMap helperargs;
    helperargs["tempkdmrcfile"] = config->name();
    helperargs["tempbackgroundrcfile"] = pBackgroundTempConfigFile->fileName();

    auto pAction = authAction();
    if (!pAction.isValid()) {
        qCritical() << "There was no authAction, not saving settings";
        return;
    }
    pAction.setArguments(helperargs);
    KAuth::ExecuteJob *j = pAction.execute();

    switch (handleKauthActionJob(this, j)) {
        case Helper::KdmrcInstallError:
            KMessageBox::error(this,
                i18n("Unable to install new kdmrc file from\n%1",
                     config->name()));
            break;
        case Helper::BackgroundrcInstallError:
            KMessageBox::error(this,
                i18n("Unable to install new backgroundrc file from\n%1",
                     pBackgroundTempConfigFile->fileName()));
            break;
        case Helper::KdmrcInstallError | Helper::BackgroundrcInstallError:
            KMessageBox::error(this,
                i18n("Unable to install new kdmrc file from\n%1"
                     "\nand new backgroundrc file from\n%2",
                     config->name(), pBackgroundTempConfigFile->fileName()));
            break;
    }

    emit changed(false);
}

void KDModule::defaults()
{
    general->defaults();
    dialog->defaults();
    background->defaults();
#ifdef KDM_THEMEABLE
    theme->defaults();
#endif
    users->defaults();
    sessions->defaults();
    convenience->defaults();
    propagateUsers();
}

void KDModule::propagateUsers()
{
    groupmap.clear();
    emit clearUsers();
    QMap<QString, int> lusers;
    QMap<QString, QPair<int, QStringList> >::const_iterator it;
    QStringList::ConstIterator jt;
    QMap<QString, int>::Iterator gmapi;
    for (it = usermap.constBegin(); it != usermap.constEnd(); ++it) {
        int uid = it.value().first;
        if (!uid || (uid >= minshowuid && uid <= maxshowuid)) {
            lusers[it.key()] = uid;
            for (jt = it.value().second.begin(); jt != it.value().second.end(); ++jt)
                if ((gmapi = groupmap.find(*jt)) == groupmap.end()) {
                    groupmap[*jt] = 1;
                    lusers['@' + *jt] = -uid;
                } else
                    (*gmapi)++;
        }
    }
    emit addUsers(lusers);
    updateOK = true;
}

void KDModule::slotMinMaxUID(int min, int max)
{
    if (updateOK) {
        QMap<QString, int> alusers, dlusers;
        QMap<QString, QPair<int, QStringList> >::const_iterator it;
        QStringList::ConstIterator jt;
        QMap<QString, int>::Iterator gmapi;
        for (it = usermap.constBegin(); it != usermap.constEnd(); ++it) {
            int uid = it.value().first;
            if (!uid)
                continue;
            if ((uid >= minshowuid && uid <= maxshowuid) &&
                    !(uid >= min && uid <= max)) {
                dlusers[it.key()] = uid;
                for (jt = it.value().second.begin();
                        jt != it.value().second.end(); ++jt) {
                    gmapi = groupmap.find(*jt);
                    if (!--(*gmapi)) {
                        groupmap.erase(gmapi);
                        dlusers['@' + *jt] = -uid;
                    }
                }
            } else if ((uid >= min && uid <= max) &&
                       !(uid >= minshowuid && uid <= maxshowuid)) {
                alusers[it.key()] = uid;
                for (jt = it.value().second.begin();
                        jt != it.value().second.end(); ++jt)
                    if ((gmapi = groupmap.find(*jt)) == groupmap.end()) {
                        groupmap[*jt] = 1;
                        alusers['@' + *jt] = -uid;
                    } else
                        (*gmapi)++;
            }
        }
        emit delUsers(dlusers);
        emit addUsers(alusers);
    }
    minshowuid = min;
    maxshowuid = max;
}

void KDModule::slotUseThemeChanged(bool use)
{
#ifdef KDM_THEMEABLE
    dialog_stack->setCurrentIndex(use);
    background_stack->setCurrentIndex(use);
    theme_stack->setCurrentIndex(use);
#endif
}

#include "main.moc"
