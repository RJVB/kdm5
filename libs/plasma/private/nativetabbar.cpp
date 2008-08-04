/*
    Copyright 2007 Robert Knight <robertknight@gmail.com>
    Copyright 2008 Marco Martin <notmart@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

// Own
#include "nativetabbar_p.h"

// KDE
#include <KDebug>
#include <KColorUtils>

// Qt
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QApplication>

#include <QGradient>
#include <QLinearGradient>

#include "plasma/plasma.h"
#include "plasma/theme.h"
#include "plasma/animator.h"
#include "plasma/panelsvg.h"


namespace Plasma
{

class NativeTabBarPrivate
{
public:
    NativeTabBarPrivate(NativeTabBar *parent)
        : q(parent),
          backgroundSvg(0),
          buttonSvg(0),
          animationId(-1)
    {
    }

    ~NativeTabBarPrivate()
    {
        delete backgroundSvg;
        delete buttonSvg;
    }

    void syncBorders();
    void storeLastIndex();

    NativeTabBar *q;
    //used to keep track of shape() changes
    QTabBar::Shape shape;
    PanelSvg *backgroundSvg;
    qreal left, top, right, bottom;
    PanelSvg *buttonSvg;
    qreal buttonLeft, buttonTop, buttonRight, buttonBottom;

    int animationId;

    QRect currentAnimRect;
    int lastIndex[2];
    qreal animProgress;
};

void NativeTabBarPrivate::syncBorders()
{
    backgroundSvg->getMargins(left, top, right, bottom);
    buttonSvg->getMargins(buttonLeft, buttonTop, buttonRight, buttonBottom);
}

void NativeTabBarPrivate::storeLastIndex()
{
    // if first run
    if (lastIndex[0] == -1) {
        lastIndex[1] = q->currentIndex();
    }
    lastIndex[0] = lastIndex[1];
    lastIndex[1] = q->currentIndex();
}



NativeTabBar::NativeTabBar(QWidget *parent)
        : QTabBar(parent),
          d(new NativeTabBarPrivate(this))
{
    d->backgroundSvg = new Plasma::PanelSvg();
    d->backgroundSvg->setImagePath("widgets/frame");
    d->backgroundSvg->setElementPrefix("sunken");

    d->buttonSvg = new Plasma::PanelSvg();
    d->buttonSvg->setImagePath("widgets/button");
    d->buttonSvg->setElementPrefix("normal");

    d->syncBorders();

    d->lastIndex[0] = -1;
    connect(this, SIGNAL(currentChanged(int)), this, SLOT(startAnimation()));

    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
}

QRect NativeTabBar::tabRect(int index) const
{
    QRect rect = QTabBar::tabRect(index).translated(d->left, d->top);

    if (isVertical()) {
        rect.setWidth(rect.width() - d->right - d->left);
    }

    return rect;
}

int NativeTabBar::lastIndex() const
{
    return d->lastIndex[0];
}

QSize NativeTabBar::tabSizeHint(int index) const
{//return QTabBar::tabSizeHint(index);
    QSize hint = tabSize(index);
    int minwidth = 0;
    int minheight = 0;

    Shape s = shape();
    switch (s) {
        case RoundedSouth:
        case TriangularSouth:
        case RoundedNorth:
        case TriangularNorth:
            if (count() > 0) {
                for (int i = count() - 1; i >= 0; i--) {
                    minwidth += tabSize(i).width();
                }
                minwidth += d->left + d->right;

                if (minwidth < width()) {
                    hint.rwidth() += (width() - minwidth) / count();
                }
            }
            break;
        case RoundedWest:
        case TriangularWest:
        case RoundedEast:
        case TriangularEast:
            if (count() > 0) {
                for (int i = count() - 1; i >= 0; i--) {
                    minheight += tabSize(i).height();
                }
                minheight += d->top + d->bottom;

                if (minheight < height()) {
                    hint.rheight() += (height() - minheight) / count();
                }
            }
            hint.rwidth() = qMax( hint.width(), width() );
            break;
    }
    return hint;
}

//FIXME: this shouldn't be necessary but it seems to return wring numbers the base implementation?
QSize NativeTabBar::sizeHint() const
{
    if (count() < 1) {
        return QTabBar::sizeHint();
    }

    int width = tabRect(0).width();
    int height = tabRect(0).height();

    if (isVertical()) {
        height *= count();
    } else {
        width *= count();
    }
    return QSize(width+d->left+d->right, height+d->top+d->bottom);
}

void NativeTabBar::paintEvent(QPaintEvent *event)
{
    if (!styleSheet().isNull()) {
        QTabBar::paintEvent(event);
        return;
    }

    QPainter painter(this);
    //int numTabs = count();
    //bool ltr = painter.layoutDirection() == Qt::LeftToRight; // Not yet used

    d->backgroundSvg->paintPanel(&painter);

    // Drawing Tabborders
    QRect movingRect;

    if (d->currentAnimRect.isNull()) {
        movingRect = tabRect(currentIndex());
    } else {
        movingRect = d->currentAnimRect;
    }

    //resizing here because in resizeevent the first time is invalid (still no tabs)
    d->buttonSvg->resizePanel(movingRect.size());
    d->buttonSvg->paintPanel(&painter, movingRect.topLeft());


    QFontMetrics metrics(painter.font());

    for (int i = 0; i < count(); i++) {
        QRect rect = tabRect(i).adjusted(d->buttonLeft, d->buttonTop,
                                         -d->buttonRight, -d->buttonBottom);
        // draw tab icon
        QRect iconRect = QRect(rect.x(), rect.y(), iconSize().width(), iconSize().height());

        iconRect.moveCenter(QPoint(iconRect.center().x(), rect.center().y()));
        tabIcon(i).paint(&painter, iconRect);

        // draw tab text
        if (i == currentIndex() && d->animProgress == 1) {
            //FIXME: theme will need a ButtonColor and ButtonTextColor
            painter.setPen(Plasma::Theme::defaultTheme()->color(Theme::BackgroundColor));
        } else {
            QColor color(Plasma::Theme::defaultTheme()->color(Theme::TextColor));
            if (!isTabEnabled(i)) {
                color.setAlpha(140);
            }

            painter.setPen(color);
        }
        QRect textRect = rect;

        if (!tabIcon(i).isNull()) {
            textRect.setLeft(iconRect.right());
        }

        painter.drawText(textRect, Qt::AlignCenter | Qt::TextHideMnemonic, tabText(i));
    }
}

void NativeTabBar::resizeEvent(QResizeEvent* event)
{
    QTabBar::resizeEvent(event);
    d->currentAnimRect = tabRect(currentIndex());
    d->backgroundSvg->resizePanel(size());

    update();
}

void NativeTabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    emit sizeHintChanged();
}

void NativeTabBar::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);
    emit sizeHintChanged();
}

void NativeTabBar::tabLayoutChange()
{
    QTabBar::tabLayoutChange();

    if (shape() != d->shape) {
        d->shape = shape();
        emit shapeChanged(d->shape);
    }
}

void NativeTabBar::startAnimation()
{
    d->storeLastIndex();
    Plasma::Animator::self()->customAnimation(10, 150, Plasma::Animator::EaseInOutCurve, this, "onValueChanged");
}

void NativeTabBar::onValueChanged(qreal value)
{
    if ((d->animProgress = value) == 1.0) {
        animationFinished();
        return;
    }

    // animation rect
    QRect rect = tabRect(currentIndex());
    QRect lastRect = tabRect(lastIndex());
    int x = isHorizontal() ? (int)(lastRect.x() - value * (lastRect.x() - rect.x())) : rect.x();
    int y = isHorizontal() ? rect.y() : (int)(lastRect.y() - value * (lastRect.y() - rect.y()));
    QSizeF sz = lastRect.size() - value * (lastRect.size() - rect.size());
    d->currentAnimRect = QRect(x, y, (int)(sz.width()), (int)(sz.height()));
    update();
}

void NativeTabBar::animationFinished()
{
    d->currentAnimRect = QRect();
    update();
}

bool NativeTabBar::isVertical() const
{
    Shape s = shape();
    if( s == RoundedWest ||
        s == RoundedEast ||
        s == TriangularWest ||
        s == TriangularEast ) {
        return true;
    }
    return false;
}

bool NativeTabBar::isHorizontal() const
{
    return !isVertical();
}

QSize NativeTabBar::tabSize(int index) const
{
    QSize hint;
    const QFontMetrics metrics(QApplication::font());
    const QSize textSize = metrics.size(Qt::TextHideMnemonic, tabText(index));
    hint.rwidth() = textSize.width() + iconSize().width();
    hint.rheight() = qMax(iconSize().height(), textSize.height());
    hint.rwidth() += d->buttonLeft + d->buttonRight;
    hint.rheight() += d->buttonTop + d->buttonBottom;
    return hint;
}

}

#include "nativetabbar_p.moc"

