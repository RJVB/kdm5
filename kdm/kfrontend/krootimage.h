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

#ifndef __KDM_KROOTIMAGE_H__
#define __KDM_KROOTIMAGE_H__

#include <bgrender.h>

#include <QApplication>
#include <QTimer>
#include <QCommandLineParser>

/**
 * In xinerama mode, each screen is rendered separately by KBackgroundRenderer.
 * This class controls a set of renderers for a desktop, and collates the
 * images. Usage is similar to KBackgroundRenderer: connect to the imageDone
 * signal.
 */
class KVirtualBGRenderer : public QObject {
    Q_OBJECT
  public:
    explicit KVirtualBGRenderer(const KSharedConfigPtr &config, QObject *parent = nullptr);
    ~KVirtualBGRenderer();

    KBackgroundRenderer * renderer(unsigned screen);
    unsigned numRenderers() const { return m_numRenderers; }

    QPixmap pixmap();

    bool needProgramUpdate();
    void programUpdate();

    bool needWallpaperChange();
    void changeWallpaper();

    void desktopResized();

    void load(bool reparseConfig = true);
    void start();
    void stop();
    void cleanup();
    void saveCacheFile();
    void enableTiling(bool enable);

  signals:
    void imageDone();

  private slots:
    void screenDone(int screen);

  private:
    QSize renderSize(int screen); // the size the renderer should be
    void initRenderers();

    KSharedConfigPtr m_pConfig;
    float m_scaleX;
    float m_scaleY;
    int m_numRenderers;
    bool m_bDrawBackgroundPerScreen;
    bool m_bCommonScreen;
    QSize m_size;

    QVector<bool> m_bFinished;
    QVector<KBackgroundRenderer *> m_renderer;
    QPixmap *m_pPixmap;
};

class MyApplication : public QApplication {
    Q_OBJECT

  public:
    MyApplication(int &argc, char **argv);
    void init(const QString &confFile);
    struct _XDisplay *display() { return dpy; }

  private Q_SLOTS:
    void renderDone();
    void slotTimeout();

  private:
    KVirtualBGRenderer *renderer = nullptr;
    QTimer timer;
    struct _XDisplay *dpy;
};

#endif // __KDM_KROOTIMAGE_H__
