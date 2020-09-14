/*

Copyright (C) 1999 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
Copyright (C) 2002,2004 Oswald Buddenhagen <ossi@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.

*/

#include "krootimage.h"
#include "config-kdm.h"

#include <bgdefaults.h>

#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <kaboutdata.h>

#include <QCommandLineOption>
#include <QDesktopWidget>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QDebug>
#include <QX11Info>
#include <QScreen>

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <stdlib.h>

KVirtualBGRenderer::KVirtualBGRenderer(const KSharedConfigPtr &config, QObject *parent)
    : QObject(parent)
{
    m_pPixmap = 0;
    m_numRenderers = 0;
    m_scaleX = 1;
    m_scaleY = 1;

    m_pConfig = config;

    initRenderers();
    m_size = QApplication::desktop()->size();
}

KVirtualBGRenderer::~KVirtualBGRenderer()
{
    for (int i = 0; i < m_numRenderers; ++i)
        delete m_renderer[i];

    delete m_pPixmap;
}

KBackgroundRenderer *
KVirtualBGRenderer::renderer(unsigned screen)
{
    return m_renderer[screen];
}

QPixmap
KVirtualBGRenderer::pixmap()
{
    if (m_numRenderers == 1) {
        return m_renderer[0]->pixmap();
    }

    return *m_pPixmap;
}

bool
KVirtualBGRenderer::needProgramUpdate()
{
    for (int i = 0; i < m_numRenderers; i++)
        if (m_renderer[i]->backgroundMode() == KBackgroundSettings::Program &&
            m_renderer[i]->KBackgroundProgram::needUpdate())
            return true;
    return false;
}

void
KVirtualBGRenderer::programUpdate()
{
    for (int i = 0; i < m_numRenderers; i++)
        if (m_renderer[i]->backgroundMode() == KBackgroundSettings::Program &&
            m_renderer[i]->KBackgroundProgram::needUpdate())
        {
            m_renderer[i]->KBackgroundProgram::update();
        }
}

bool
KVirtualBGRenderer::needWallpaperChange()
{
    for (int i = 0; i < m_numRenderers; i++)
        if (m_renderer[i]->needWallpaperChange())
            return true;
    return false;
}

void
KVirtualBGRenderer::changeWallpaper()
{
    for (int i = 0; i < m_numRenderers; i++)
        m_renderer[i]->changeWallpaper();
}

void
KVirtualBGRenderer::desktopResized()
{
    m_size = QApplication::desktop()->size();

    if (m_pPixmap) {
        delete m_pPixmap;
        m_pPixmap = new QPixmap(m_size);
        m_pPixmap->fill(Qt::black);
    }

    for (int i = 0; i < m_numRenderers; i++)
        m_renderer[i]->desktopResized();
}


QSize
KVirtualBGRenderer::renderSize(int screen)
{
    return m_bDrawBackgroundPerScreen ?
           QApplication::desktop()->screenGeometry(screen).size() :
           QApplication::desktop()->size();
}


void
KVirtualBGRenderer::initRenderers()
{
    KConfigGroup cg(m_pConfig, "Background Common");
    // Same config for each screen?
    // FIXME: Could use same renderer for identically-sized screens.
    m_bCommonScreen = cg.readEntry("CommonScreen", _defCommonScreen);
    // Do not split one big image over all screens?
    m_bDrawBackgroundPerScreen =
        cg.readEntry("DrawBackgroundPerScreen_0", _defDrawBackgroundPerScreen);

    m_numRenderers = m_bDrawBackgroundPerScreen ? QApplication::desktop()->numScreens() : 1;

    m_bFinished.resize(m_numRenderers);
    m_bFinished.fill(false);

    if (m_numRenderers == m_renderer.size())
        return;

    qDeleteAll(m_renderer);
    m_renderer.resize(m_numRenderers);
    for (int i = 0; i < m_numRenderers; i++) {
        int eScreen = m_bCommonScreen ? 0 : i;
        KBackgroundRenderer *r = new KBackgroundRenderer(eScreen, m_bDrawBackgroundPerScreen, m_pConfig);
        m_renderer.insert(i, r);
        r->setSize(renderSize(i));
        connect(r, SIGNAL(imageDone(int)), SLOT(screenDone(int)));
    }
    qDebug() << Q_FUNC_INFO << "Initialised renderers:" << m_numRenderers;
}

void
KVirtualBGRenderer::load(bool reparseConfig)
{
    initRenderers();

    for (int i = 0; i < m_numRenderers; i++) {
        int eScreen = m_bCommonScreen ? 0 : i;
        m_renderer[i]->load(eScreen, m_bDrawBackgroundPerScreen, reparseConfig);
    }
}

void
KVirtualBGRenderer::screenDone(int screen)
{
    m_bFinished[screen] = true;

    if (m_pPixmap) {
        // There's more than one renderer, so we are drawing each output to our own pixmap

        QRect overallGeometry;
        for (int i = 0; i < QApplication::desktop()->numScreens(); i++)
            overallGeometry |= QApplication::desktop()->screenGeometry(i);

        QPoint drawPos =
            QApplication::desktop()->screenGeometry(screen).topLeft() -
            overallGeometry.topLeft();
        drawPos.setX(int(drawPos.x() * m_scaleX));
        drawPos.setY(int(drawPos.y() * m_scaleY));

        QPixmap source = m_renderer[screen]->pixmap();
        qDebug() << Q_FUNC_INFO << "Pixmap for screen" << screen << ":" << source;
        QSize renderSize = this->renderSize(screen);
        renderSize.setWidth(int(renderSize.width() * m_scaleX));
        renderSize.setHeight(int(renderSize.height() * m_scaleY));

        QPainter p(m_pPixmap);

        if (renderSize == source.size())
            p.drawPixmap(drawPos, source);
        else
            p.drawTiledPixmap(drawPos.x(), drawPos.y(),
                              renderSize.width(), renderSize.height(), source);

        p.end();
    }

    for (int i = 0; i < m_bFinished.size(); i++)
        if (!m_bFinished[i])
            return;

    emit imageDone();
}

void
KVirtualBGRenderer::start()
{
    delete m_pPixmap;
    m_pPixmap = 0;

    if (m_numRenderers > 1) {
        m_pPixmap = new QPixmap(m_size);
        // If are screen sizes do not properly tile the overall virtual screen
        // size, then we want the untiled parts to be black for use in desktop
        // previews, etc
        m_pPixmap->fill(Qt::black);
    }

    m_bFinished.fill(false);
    for (int i = 0; i < m_numRenderers; i++)
        m_renderer[i]->start();
}


void KVirtualBGRenderer::stop()
{
    for (int i = 0; i < m_numRenderers; i++)
        m_renderer[i]->stop();
}


void KVirtualBGRenderer::cleanup()
{
    m_bFinished.fill(false);

    for (int i = 0; i < m_numRenderers; i++)
        m_renderer[i]->cleanup();

    delete m_pPixmap;
    m_pPixmap = 0l;
}

void KVirtualBGRenderer::saveCacheFile()
{
    for (int i = 0; i < m_numRenderers; i++)
        m_renderer[i]->saveCacheFile();
}

void KVirtualBGRenderer::enableTiling(bool enable)
{
    for (int i = 0; i < m_numRenderers; i++)
        m_renderer[i]->enableTiling(enable);
}

MyApplication::MyApplication(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setApplicationVersion(QStringLiteral(KDM5_VERSION));
    dpy = QX11Info::display();
}

void MyApplication::init(const QString &confFile)
{
    renderer = new KVirtualBGRenderer(KSharedConfig::openConfig(QFile::decodeName(confFile.toUtf8())), this);
    connect(&timer, SIGNAL(timeout()), SLOT(slotTimeout()));
    connect(renderer, SIGNAL(imageDone()), this, SLOT(renderDone()));
    renderer->enableTiling(true); // optimize
    renderer->changeWallpaper(); // cannot do it when we're killed, so do it now
    timer.start(60000);
    renderer->start();
}

void
/**
 * @brief ...
 * 
 */
MyApplication::renderDone()
{
    QPalette palette = desktop()->palette();
    const QPixmap bgPM = renderer->pixmap();
    palette.setBrush(desktop()->backgroundRole(), QBrush(bgPM));
    desktop()->setPalette(palette);
    QApplication::setPalette(palette);
    if (dpy) {
        // this should do the trick, did so in Qt4, but doesn't any longer in Qt5:
        XClearWindow(dpy, desktop()->winId());
        auto rect = desktop()->screenGeometry();
        auto _connection = QX11Info::connection();
        Pixmap dpm = XCreatePixmap(dpy, desktop()->winId(), rect.width(), rect.height(), desktop()->depth());
        auto _gc = xcb_generate_id(_connection);
        xcb_create_gc(_connection, _gc, dpm, 0, nullptr);
        QImage image(bgPM.toImage());
        const auto h = image.height(), w = image.width();
        const auto d = desktop()->depth();
        const auto dwin = desktop()->winId();
        // There must be a more "elegant" way to achieve this, but at some level it will boil down
        // to something like this; a 2D loop that blits the image over the entire window.
        for (int y = 0; y < rect.height(); y += h) {
            for (int x = 0; x < rect.width(); x += w) {
                xcb_put_image(
                    _connection, XCB_IMAGE_FORMAT_Z_PIXMAP, dwin, _gc,
                    w, h, x, y,
                    0, d,
                    image.byteCount(), image.constBits());
            }
        }
        xcb_free_gc(_connection, _gc);
        XFreePixmap(dpy, dpm);
    }

    renderer->saveCacheFile();
    renderer->cleanup();
    for (unsigned i = 0; i < renderer->numRenderers(); ++i) {
        KBackgroundRenderer *r = renderer->renderer(i);
        if (r->backgroundMode() == KBackgroundSettings::Program ||
            (r->multiWallpaperMode() != KBackgroundSettings::NoMulti &&
             r->multiWallpaperMode() != KBackgroundSettings::NoMultiRandom))
            return;
    }
    QTimer::singleShot(5000, this, &MyApplication::quit);
//     quit();
}

void
MyApplication::slotTimeout()
{
    bool change = false;

    if (renderer->needProgramUpdate()) {
        renderer->programUpdate();
        change = true;
    }

    if (renderer->needWallpaperChange()) {
        renderer->changeWallpaper();
        change = true;
    }

    if (change)
        renderer->start();
}

int
main(int argc, char *argv[])
{
    MyApplication app(argc, argv);

    QCommandLineParser parser;
    
    KLocalizedString::setApplicationDomain("kdmgreet");

    KAboutData about("krootimage", i18n("KRootImage"), QStringLiteral(KDM5_VERSION),
                       i18n("Fancy desktop background for kdm"), KAboutLicense::GPL,
                       i18n("(c) 1996-2010 The KDM Authors"), QString(),
                       QStringLiteral("http://developer.kde.org/~ossi/sw/kdm.html"));
    about.setupCommandLine(&parser);

    parser.addPositionalArgument(QStringLiteral("config"), i18n("Name of the configuration file"));
    parser.addVersionOption();

    parser.process(app);
    about.processCommandLine(&parser);

    const auto args = parser.positionalArguments();
    if (args.count() > 0) {
        app.init(args.first());
    }

    if (app.display()) {
        // Keep color resources after termination
        XSetCloseDownMode(app.display(), RetainTemporary);
    }

    const auto ret = app.exec();

    if (app.display()) {
        app.flush();
    }

    return ret;
}

#include "moc_krootimage.cpp"
