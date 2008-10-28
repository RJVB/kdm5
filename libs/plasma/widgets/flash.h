/*
 *   Copyright 2007 by André Duffeck <duffeck@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.

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

#ifndef PLASMA_FLASH_H
#define PLASMA_FLASH_H

#include <QtGui/QGraphicsWidget>
#include <QtGui/QTextOption>

#include <plasma/plasma_export.h>

namespace Plasma
{

class FlashPrivate;

/**
 * @class Flash plasma/widgets/flash.h <Plasma/Widgets/Flash>
 *
 * @short Provides flashing text or icons inside Plasma
 */
class PLASMA_EXPORT Flash : public QGraphicsWidget
{
    Q_OBJECT
    public:
        explicit Flash(QGraphicsItem *parent = 0);
        virtual ~Flash();

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

        void setFont(const QFont &);
        void setColor(const QColor &);
        void setDuration(int duration);

        void flash(const QString &text, int duration = 0,
                   const QTextOption &option = QTextOption(Qt::AlignCenter));
        void flash(const QPixmap &pixmap, int duration = 0,
                   Qt::Alignment align = Qt::AlignCenter);

        void setAutohide(bool autohide);
        bool autohide() const;

    public Q_SLOTS:
        void kill();

    protected Q_SLOTS:
        void fadeIn();
        void fadeOut();

    private:
        Q_PRIVATE_SLOT(d, void elementAnimationFinished(int))
        FlashPrivate *const d;
};

}

#endif
