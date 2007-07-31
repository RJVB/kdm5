/*
 *   Copyright (C) 2005 by Aaron Seigo <aseigo@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "applet.h"

#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QList>
#include <QPainter>
#include <QSize>
#include <QStyleOptionGraphicsItem>
#include <QTimer>
#include <QUiLoader>

#include <KIcon>
#include <KConfigDialog>
#include <KDialog>
#include <KPluginInfo>
#include <KStandardDirs>
#include <KService>
#include <KServiceTypeTrader>
#include <KIconLoader>

#include "plasma/configxml.h"
#include "plasma/corona.h"
#include "plasma/dataenginemanager.h"
#include "plasma/package.h"
#include "plasma/packages_p.h"
#include "plasma/plasma.h"
#include "plasma/scriptengine.h"
#include "plasma/svg.h"

#include "plasma/widgets/widget.h"
#include "plasma/widgets/lineedit.h"
#include "plasma/widgets/vboxlayout.h"

//#include "stackblur_shadows.cpp"

namespace Plasma
{

class Applet::Private
{
public:
    Private(KService::Ptr service, int uniqueID)
        : appletId(uniqueID),
          globalConfig(0),
          appletConfig(0),
          appletDescription(service),
          package(0),
          background(0),
          failureText(0),
          scriptEngine(0),
          configXml(0),
          kioskImmutable(false),
          immutable(false),
          hasConfigurationInterface(false),
          failed(false),
          cachedBackground(0)
    {
        if (appletId == 0) {
            appletId = nextId();
        }

        if (appletId > s_maxAppletId) {
            s_maxAppletId = appletId;
        }
    }

    ~Private()
    {
        foreach ( const QString& engine, loadedEngines ) {
            DataEngineManager::self()->unloadDataEngine( engine );
        }
        delete background;
        delete package;
        delete configXml;
        delete cachedBackground;
    }

    void init(Applet* applet)
    {
        applet->setFlag(QGraphicsItem::ItemClipsToShape, false);
        applet->setFlag(QGraphicsItem::ItemClipsChildrenToShape, false);
        kioskImmutable = applet->globalConfig().isImmutable() ||
                         applet->config().isImmutable();
        applet->setImmutable(kioskImmutable);

        if (!appletDescription.isValid()) {
            applet->setFailedToLaunch(true);
            return;
        }

        QString language = appletDescription.property("X-Plasma-Language").toString();

        // we have a scripted plasmoid
        if (!language.isEmpty()) {
            // find where the Package is
            QString path = KStandardDirs::locate("appdata",
                                                 "plasmoids/" +
                                                 appletDescription.pluginName());

            if (!path.isEmpty()) {
                // create the package and see if we have something real
                package = new Package(path,
                                      appletDescription.pluginName(),
                                      PlasmoidStructure());
                if (package->isValid()) {
                    // now we try and set up the script engine.
                    // it will be parented to this applet and so will get
                    // deleted when the applet does

                    scriptEngine = ScriptEngine::load(language, applet);
                    if (!scriptEngine) {
                        delete package;
                        package = 0;
                    }
                } else {
                    delete package;
                    package = 0;
                }

                if (!package) {
                    applet->setFailedToLaunch(true);
                } else {
                    setupScriptSupport(applet);
                }
            }
        }
    }

    // put all setup routines for script here. at this point we can assume that
    // package exists and that we have a script engin
    void setupScriptSupport(Applet* applet)
    {
        Q_ASSERT(package);
        QString xmlPath = package->filePath("mainconfigxml");
        if (!xmlPath.isEmpty()) {
            QFile file(xmlPath);
            configXml = new ConfigXml(config(), &file);
        }

        if (!package->filePath("mainconfigui").isEmpty()) {
            applet->setHasConfigurationInterface(true);
        }
    }

    void paintBackground(QPainter* p2, Applet* q)
    {
        QSize contents = contentSize(q).toSize();
        const int contentWidth = contents.width();
        const int contentHeight = contents.height();

        background->resize();

        const int topHeight = background->elementSize("top").height();
        const int topWidth = background->elementSize("top").width();
        const int leftWidth = background->elementSize("left").width();
        const int leftHeight = background->elementSize("left").height();
        const int rightWidth = background->elementSize("right").width();
        const int bottomHeight = background->elementSize("bottom").height();
        //const int lrWidths = leftWidth + rightWidth;
        //const int tbHeights = topHeight + bottomHeight;

        // contents top-left corner is (0,0).  We need to draw up and left of that
        const int topOffset = 0 - topHeight;
        const int leftOffset = 0 - leftWidth;
        const int rightOffset = contentWidth;
        const int bottomOffset = contentHeight;
        const int contentTop = 0;
        const int contentLeft = 0;
        if (!cachedBackground || cachedBackground->size() != QSize(leftWidth + contentWidth + rightWidth, topHeight + contentHeight + bottomHeight)) {
            delete cachedBackground;
            cachedBackground = new QPixmap(leftWidth + contentWidth + rightWidth, topHeight + contentHeight + bottomHeight);
            cachedBackground->fill(Qt::transparent);
            QPainter p(cachedBackground);
            p.translate(leftWidth, topHeight);
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.setRenderHint(QPainter::SmoothPixmapTransform);

            background->paint(&p, QRect(leftOffset,  topOffset,    leftWidth,  topHeight),    "topleft");
            background->paint(&p, QRect(rightOffset, topOffset,    rightWidth, topHeight),    "topright");
            background->paint(&p, QRect(leftOffset,  bottomOffset, leftWidth,  bottomHeight), "bottomleft");
            background->paint(&p, QRect(rightOffset, bottomOffset, rightWidth, bottomHeight), "bottomright");

            QPixmap left(leftWidth, leftHeight);
            left.fill(Qt::transparent);
            {
                QPainter sidePainter(&left);
                sidePainter.setCompositionMode(QPainter::CompositionMode_Source);
                background->paint(&sidePainter, QPoint(0, 0), "left");
            }
            p.drawTiledPixmap(QRect(leftOffset, contentTop, leftWidth, contentHeight), left);

            QPixmap right(rightWidth, leftHeight);
            right.fill(Qt::transparent);
            {
                QPainter sidePainter(&right);
                sidePainter.setCompositionMode(QPainter::CompositionMode_Source);
                background->paint(&sidePainter, QPoint(0, 0), "right");
            }
            p.drawTiledPixmap(QRect(rightOffset, contentTop, rightWidth, contentHeight), right);

            QPixmap top(topWidth, topHeight);
            top.fill(Qt::transparent);
            {
                QPainter sidePainter(&top);
                sidePainter.setCompositionMode(QPainter::CompositionMode_Source);
                background->paint(&sidePainter, QPoint(0, 0), "top");
            }
            p.drawTiledPixmap(QRect(contentLeft, topOffset, contentWidth, topHeight), top);

            QPixmap bottom(topWidth, bottomHeight);
            bottom.fill(Qt::transparent);
            {
                QPainter sidePainter(&bottom);
                sidePainter.setCompositionMode(QPainter::CompositionMode_Source);
                background->paint(&sidePainter, QPoint(0, 0), "bottom");
            }
            p.drawTiledPixmap(QRect(contentLeft, bottomOffset, contentWidth, bottomHeight), bottom);

            background->paint(&p, QRect(contentLeft, contentTop, contentWidth + 1, contentHeight + 1), "center");
            p.end();
        }
        p2->drawPixmap(leftOffset, topOffset, *cachedBackground);
    }

    void paintHover(QPainter* painter, Applet* q)
    {
        //TODO draw hover interface for close, configure, info and move
    }

    QSizeF contentSize(const Applet* q)
    {
        if (scriptEngine) {
            return scriptEngine->size();
        }

        if (failureText) {
            return failureText->geometry().size();
        }

        return q->contentSize();
    }

    static uint nextId()
    {
        ++s_maxAppletId;
        return s_maxAppletId;
    }

    KSharedConfig::Ptr config() {
        if (!appletConfig) {
            QString file = KStandardDirs::locateLocal("appdata",
                                                      "applets/" + instanceName() + "rc",
                                                      true);
            appletConfig = KSharedConfig::openConfig(file);
        }

        return appletConfig;
    }

    QString instanceName()
    {
        if (!appletDescription.isValid()) {
            return QString();
        }

        return appletDescription.service()->library() + QString::number(appletId);
    }

    //TODO: examine the usage of memory here; there's a pretty large
    //      number of members at this point.
    uint appletId;
    KSharedConfig::Ptr globalConfig;
    KSharedConfig::Ptr appletConfig;
    KPluginInfo appletDescription;
    Package* package;
    QList<QObject*> watchedForFocus;
    QStringList loadedEngines;
    static uint s_maxAppletId;
    Plasma::Svg *background;
    Plasma::LineEdit *failureText;
    ScriptEngine* scriptEngine;
    ConfigXml* configXml;
    bool kioskImmutable : 1;
    bool immutable : 1;
    bool hasConfigurationInterface : 1;
    bool failed : 1;
private:
    QPixmap* cachedBackground;
};

uint Applet::Private::s_maxAppletId = 0;

Applet::Applet(QGraphicsItem *parent,
               const QString& serviceID,
               uint appletId)
    : QObject(0),
      Widget(parent),
      d(new Private(KService::serviceByStorageId(serviceID), appletId))
{
    d->init(this);
}

Applet::Applet(QObject* parent, const QStringList& args)
    : QObject(parent),
      Widget(0),
      d(new Private(KService::serviceByStorageId(args.count() > 0 ? args[0] : QString()),
                    args.count() > 1 ? args[1].toInt() : 0))
{
    d->init(this);
    // the brain damage seen in the initialization list is due to the 
    // inflexibility of KService::createInstance
}

Applet::~Applet()
{
    needsFocus( false );
    delete d;
}

KConfigGroup Applet::config() const
{
    return KConfigGroup(d->config(), "General");
}

KConfigGroup Applet::config(const QString& group) const
{
    KConfigGroup cg = config();
    cg.changeGroup(instanceName() + '-' + group);
    return cg;
}

KConfigGroup Applet::globalConfig() const
{
    if ( !d->globalConfig ) {
        QString file = KStandardDirs::locateLocal( "config", "plasma_" + globalName() + "rc" );
        d->globalConfig = KSharedConfig::openConfig( file );
    }

    return KConfigGroup(d->globalConfig, "General");
}

ConfigXml* Applet::configXml()
{
    return d->configXml;
}

DataEngine* Applet::dataEngine(const QString& name) const
{
    int index = d->loadedEngines.indexOf(name);
    if (index != -1) {
        return DataEngineManager::self()->dataEngine(name);
    }

    DataEngine* engine = DataEngineManager::self()->loadDataEngine(name);
    if (engine->isValid()) {
        d->loadedEngines.append(name);
    }

    return engine;
}

const Package* Applet::package() const
{
    return d->package;
}

void Applet::constraintsUpdated()
{
    kDebug() << "Applet::constraintsUpdate(): constraints are FormFactor: " << formFactor() << ", Location: " << location() << endl;
}

QString Applet::name() const
{
    if (!d->appletDescription.isValid()) {
        return i18n("Unknown Applet");
    }

    return d->appletDescription.name();
}

QString Applet::icon() const
{
    if (!d->appletDescription.isValid()) {
        return QString();
    }

    return d->appletDescription.icon();
}

QString Applet::category() const
{
    if (!d->appletDescription.isValid()) {
        return i18n("Misc");
    }

    return d->appletDescription.property("X-KDE-PluginInfo-Category").toString();
}

QString Applet::category(const KPluginInfo& applet)
{
    return applet.property("X-KDE-PluginInfo-Category").toString();
}

QString Applet::category(const QString& appletName)
{
    if (appletName.isEmpty()) {
        return QString();
    }

    QString constraint = QString("[X-KDE-PluginInfo-Name] == '%1'").arg(appletName);
    KService::List offers = KServiceTypeTrader::self()->query("Plasma/Applet", constraint);

    if (offers.isEmpty()) {
        return QString();
    }

    return offers.first()->property("X-KDE-PluginInfo-Category").toString();
}

bool Applet::isImmutable() const
{
    return d->immutable || d->kioskImmutable;
}

void Applet::setImmutable(bool immutable)
{
    d->immutable = immutable;
    setFlag(QGraphicsItem::ItemIsMovable, d->immutable || d->kioskImmutable ||
                                          !scene() || !static_cast<Corona*>(scene())->isImmutable());
}

bool Applet::drawStandardBackground()
{
    return d->background != 0;
}

void Applet::setDrawStandardBackground(bool drawBackground)
{
    if (drawBackground) {
        if (!d->background) {
            prepareGeometryChange();
            d->background = new Plasma::Svg("widgets/background");
        }
    } else if (d->background) {
        prepareGeometryChange();
        delete d->background;
        d->background = 0;
    }
}

bool Applet::failedToLaunch() const
{
    return d->failed;
}

QString visibleFailureText(const QString& reason)
{
    QString text;

    if (reason.isEmpty()) {
        text = i18n("This object could not be created.");
    } else {
        text = i18n("This object could not be created for the following reason:<p>%1</p>", reason);
    }

    return text;
}

void Applet::setFailedToLaunch(bool failed, const QString& reason)
{
    if (d->failed == failed) {
        if (d->failureText) {
            d->failureText->setHtml(visibleFailureText(reason));
        }
        return;
    }

    d->failed = failed;
    prepareGeometryChange();
    qDeleteAll(QGraphicsItem::children());
    delete layout();

    if (failed) {
        setDrawStandardBackground(failed || d->background != 0);
        Layout* failureLayout = new VBoxLayout(this);
        d->failureText = new LineEdit(this, scene());
        d->failureText->setFlags(0);
        d->failureText->setHtml(visibleFailureText(reason));
        failureLayout->addItem(d->failureText);
    } else {
        d->failureText = 0;
    }

    update();
}

int Applet::type() const
{
    return Type;
}

QRectF Applet::boundingRect() const
{
    QRectF rect = QRectF(QPointF(0,0), d->contentSize(this));
    if (!d->background) {
        return rect;
    }

    const int topHeight = d->background->elementSize("top").height();
    const int leftWidth = d->background->elementSize("left").width();
    const int rightWidth = d->background->elementSize("right").width();
    const int bottomHeight = d->background->elementSize("bottom").height();

    rect.adjust(0 - leftWidth, 0 - topHeight, rightWidth, bottomHeight);
    return rect;
}

QList<QAction*> Applet::contextActions()
{
    return QList<QAction*>();
}

QColor Applet::color() const
{
    // TODO: add more colors for more categories and
    // maybe read from config?
    QString c = category();
    int alpha = 200;
    // Colors taken from Oxygen color palette
    if (c == "Date and Time") {
        return QColor(191, 94, 0, alpha);
    } else if (c == "Environment & Weather") {
        return QColor(191, 0, 0, alpha);
    } else if (c == "Examples") {
        return QColor(204, 0, 154, alpha);
    } else if (c == "File System") {
        return QColor(90, 0, 179, alpha);
    } else if (c == "Graphics") {
        return QColor(0, 0, 255, alpha);
    } else if (c == "Language") {
        return QColor(0, 191, 0, alpha);
    } else if (c == "Mapping") {
        return QColor(191, 245, 0, alpha);
    } else if (c == "Online Services") {
        return QColor(255, 213, 0, alpha);
    } else if (c == "System Information") {
        return QColor(0, 196, 204, alpha);
    } else if (c == "Windows and Tasks") {
        return QColor(255, 126, 0, alpha);
    } else {
        return QColor(136, 136, 136, alpha);
    }
}

void Applet::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)
    qreal zoomLevel = painter->transform().m11() / transform().m11();
    if (zoomLevel == scalingFactor(Plasma::DesktopZoom)) { // Show Desktop
        if (d->background) {
            d->paintBackground(painter, this);
        }

        if (d->failed) {
            return;
        }

        paintInterface(painter, option, QRect(QPoint(0,0), d->contentSize(this).toSize()));
        d->paintHover(painter, this);
    } else if (zoomLevel == scalingFactor(Plasma::GroupZoom)) { // Show Groups + Applet outline
        //TODO: make pretty.
        painter->setBrush(QBrush(color(), Qt::SolidPattern));
        painter->drawRoundRect(boundingRect());
        int iconDim = KIconLoader().currentSize(K3Icon::Desktop);
        int midX = (boundingRect().width() / 2) - (iconDim / 2);
        int midY = (boundingRect().height() / 2) - (iconDim / 2);
        KIcon(icon()).paint(painter, midX, midY, iconDim, iconDim);
    }/*  else if (zoomLevel == scalingFactor(Plasma::OverviewZoom)) { //Show Groups only
    } */
}

void Applet::paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option,
                            const QRect & contentsRect)
{
    Q_UNUSED(contentsRect)

    if (d->scriptEngine) {
        d->scriptEngine->paintInterface(painter, option);
    } else {
        //kDebug() << "Applet::paintInterface() default impl" << endl;
    }
}

FormFactor Applet::formFactor() const
{
    if (!scene()) {
        return Plasma::Planar;
    }

    return static_cast<Corona*>(scene())->formFactor();
}

Location Applet::location() const
{
    if (!scene()) {
        return Plasma::Desktop;
    }

    return static_cast<Corona*>(scene())->location();
}

QSizeF Applet::contentSize() const
{
    if (layout()) {
        return layout()->sizeHint();
    }

    return QSizeF(0, 0);
}

QString Applet::globalName() const
{
    if (!d->appletDescription.isValid()) {
        return QString();
    }

    return d->appletDescription.service()->library();
}

QString Applet::instanceName() const
{
    return d->instanceName();
}

void Applet::watchForFocus(QObject *widget, bool watch)
{
    if ( !widget ) {
        return;
    }

    int index = d->watchedForFocus.indexOf(widget);
    if ( watch ) {
        if ( index == -1 ) {
            d->watchedForFocus.append( widget );
            widget->installEventFilter( this );
        }
    } else if ( index != -1 ) {
        d->watchedForFocus.removeAt( index );
        widget->removeEventFilter( this );
    }
}

void Applet::needsFocus(bool focus)
{
    if (focus == QGraphicsItem::hasFocus()) {
        return;
    }

    emit requestFocus(focus);
}

bool Applet::hasConfigurationInterface()
{
    return d->hasConfigurationInterface;
}

void Applet::setHasConfigurationInterface(bool hasInterface)
{
    d->hasConfigurationInterface = hasInterface;
}

bool Applet::eventFilter( QObject *o, QEvent * e )
{
    if ( !d->watchedForFocus.contains( o ) )
    {
        if ( e->type() == QEvent::MouseButtonRelease ||
             e->type() == QEvent::FocusIn ) {
            needsFocus( true );
        } else if ( e->type() == QEvent::FocusOut ) {
            needsFocus( false );
        }
    }

    return QObject::eventFilter(o, e);
}

void Applet::showConfigurationInterface()
{
    if (d->package && d->configXml) {
        QString uiFile = d->package->filePath("mainconfigui");
        if (uiFile.isEmpty()) {
            return;
        }

        KConfigDialog *dialog = new KConfigDialog(0, "", d->configXml);
        dialog->setWindowTitle(i18n("%1 Settings", name()));
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);

        QUiLoader loader;
        QString filename = d->package->filePath("mainconfigui");
        QFile f(filename);
        if (!f.open(QIODevice::ReadOnly)) {
            delete dialog;
            return;
        }

        QWidget *w = loader.load(&f);
        f.close();

        dialog->addPage(w, i18n("Settings"), icon(), i18n("%1 Settings", name()));
        dialog->show();
    }
}

KPluginInfo::List Applet::knownApplets(const QString &category,
                                       const QString &parentApp)
{
    QString constraint;

    if (parentApp.isEmpty()) {
        constraint.append("not exist [X-KDE-ParentApp]");
    } else {
        constraint.append("[X-KDE-ParentApp] == '").append(parentApp).append("'");
    }

    if (!category.isEmpty()) {
        if (!constraint.isEmpty()) {
            constraint.append(" and ");
        }

        constraint.append("[X-KDE-PluginInfo-Category] == '").append(category).append("'");
        if (category == "Misc") {
            constraint.append(" or (not exist [X-KDE-PluginInfo-Category] or [X-KDE-PluginInfo-Category] == '')");
        }
    }

    KService::List offers = KServiceTypeTrader::self()->query("Plasma/Applet", constraint);
    //kDebug() << "Applet::knownApplets constraint was '" << constraint << "' which got us " << offers.count() << " matches" << endl;
    return KPluginInfo::fromServices(offers);
}

QStringList Applet::knownCategories(const QString &parentApp)
{
    QString constraint = "exist [X-KDE-PluginInfo-Category]";

    if (parentApp.isEmpty()) {
        constraint.append(" and not exist [X-KDE-ParentApp]");
    } else {
        constraint.append(" and [X-KDE-ParentApp] == '").append(parentApp).append("'");
    }

    KService::List offers = KServiceTypeTrader::self()->query("Plasma/Applet", constraint);
    QStringList categories;
    foreach (KService::Ptr applet, offers) {
        QString appletCategory = applet->property("X-KDE-PluginInfo-Category").toString();
        kDebug() << "   and we have " << appletCategory << endl;
        if (appletCategory.isEmpty()) {
            if (!categories.contains(i18n("Misc"))) {
                categories << i18n("Misc");
            }
        } else  if (!categories.contains(appletCategory)) {
            categories << appletCategory;
        }
    }

    categories.sort();
    return categories;
}

Applet* Applet::loadApplet(const QString& appletName, uint appletId, const QStringList& args)
{
    if (appletName.isEmpty()) {
        return 0;
    }

    QString constraint = QString("[X-KDE-PluginInfo-Name] == '%1'").arg(appletName);
    KService::List offers = KServiceTypeTrader::self()->query("Plasma/Applet", constraint);

    if (offers.isEmpty()) {
        //TODO: what would be -really- cool is offer to try and download the applet
        //      from the network at this point
        kDebug() << "Applet::loadApplet: offers is empty for \"" << appletName << "\"" << endl;
        return 0;
    }

    if (appletId == 0) {
        appletId = Private::nextId();
    }

    QStringList allArgs;
    QString id;
    id.setNum(appletId);
    allArgs << offers.first()->storageId() << id << args;
    Applet* applet = KService::createInstance<Plasma::Applet>(offers.first(), 0, allArgs);

    if (!applet) {
        kDebug() << "Couldn't load applet \"" << appletName << "\"!" << endl;
    }

    return applet;
}

Applet* Applet::loadApplet(const KPluginInfo& info, uint appletId, const QStringList& args)
{
    if (!info.isValid()) {
        return 0;
    }

    return loadApplet(info.pluginName(), appletId, args);
}

} // Plasma namespace

#include "applet.moc"
