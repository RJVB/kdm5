/*
 *   Copyright 2008 Marco Martin <notmart@gmail.com>
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

#include "tabbar.h"

#include <QGraphicsLinearLayout>
#include <QGraphicsLayoutItem>
#include <QString>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneWheelEvent>
#include <QIcon>
#include <KDebug>

#include <plasma/animator.h>
#include <plasma/panelsvg.h>
#include <plasma/theme.h>

#include "private/nativetabbar_p.h"

namespace Plasma
{

class TabBarPrivate
{
public:
    TabBarPrivate(TabBar *parent)
        : q(parent),
          tabBar(0),
          currentIndex(0),
          oldPage(0),
          newPage(0),
          oldPageAnimId(-1),
          newPageAnimId(-1)
    {
    }

    ~TabBarPrivate()
    {
    }

    void syncBorders();
    void slidingCompleted(QGraphicsItem *item);
    void shapeChanged(const QTabBar::Shape shape);

    TabBar *q;
    NativeTabBar *tabBar;
    PanelSvg *background;
    QList<QGraphicsWidget *> pages;
    QGraphicsLinearLayout *mainLayout;
    QGraphicsLinearLayout *tabBarLayout;
    int currentIndex;

    QGraphicsWidget *oldPage;
    QGraphicsWidget *newPage;
    int oldPageAnimId;
    int newPageAnimId;
};

void TabBarPrivate::syncBorders()
{
    //set margins from the normal element
    qreal left, top, right, bottom;

    background->getMargins(left, top, right, bottom);

    q->setContentsMargins(left, top, right, bottom);
}

void TabBarPrivate::slidingCompleted(QGraphicsItem *item)
{
    if (item == oldPage || item == newPage) {
        if (item == newPage) {
            mainLayout->addItem(newPage);
            newPageAnimId = -1;
        } else {
            oldPageAnimId = -1;
            item->hide();
        }
        q->setFlags(0);
    }
}

void TabBarPrivate::shapeChanged(const QTabBar::Shape shape)
{
    //FIXME: QGraphicsLinearLayout doesn't have setDirection, so for now
    // North is equal to south and East is equal to West
    switch (shape) {
    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:

    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        mainLayout->setOrientation(Qt::Horizontal);
        mainLayout->itemAt(0)->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        break;

    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:

    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:
    default:
        mainLayout->setOrientation(Qt::Vertical);
        mainLayout->itemAt(0)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
}


TabBar::TabBar(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
      d(new TabBarPrivate(this))
{
    d->tabBar = new NativeTabBar();
    d->tabBar->setAttribute(Qt::WA_NoSystemBackground);
    d->mainLayout = new QGraphicsLinearLayout(Qt::Vertical);
    d->tabBarLayout = new QGraphicsLinearLayout(Qt::Horizontal);

    setLayout(d->mainLayout);

    d->mainLayout->addItem(d->tabBarLayout);

    QGraphicsProxyWidget *tabProxy = new QGraphicsProxyWidget(this);
    tabProxy->setWidget(d->tabBar);
    tabProxy->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    //tabBar are centered, so a stretch at begin one at the end
    d->tabBarLayout->addStretch();
    d->tabBarLayout->addItem(tabProxy);
    d->tabBarLayout->addStretch();

    //background painting stuff
    d->background = new Plasma::PanelSvg(this);
    d->background->setImagePath("widgets/frame");
    d->background->setElementPrefix("sunken");

    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), SLOT(syncBorders()));
    
    connect(d->tabBar, SIGNAL(currentChanged(int)), this, SLOT(setCurrentIndex(int)));
    connect(d->tabBar, SIGNAL(shapeChanged(QTabBar::Shape)), this, SLOT(shapeChanged(QTabBar::Shape)));
    connect(Plasma::Animator::self(), SIGNAL(movementFinished(QGraphicsItem*)), this, SLOT(slidingCompleted(QGraphicsItem*)));
}

TabBar::~TabBar()
{
    delete d;
}

int TabBar::insertTab(int index, const QIcon &icon, const QString &label, QGraphicsLayoutItem *content)
{
    QGraphicsWidget *page = new QGraphicsWidget(this);
    page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (content) {
        QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical, page);
        layout->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        page->setLayout(layout);
        layout->addItem(content);
    } else {
        page->setMaximumHeight(0);
    }

    d->pages.insert(qBound(0, index, d->pages.count()), page);

    if (d->pages.count() == 1) {
        d->mainLayout->addItem(page);
        page->setVisible(true);
        page->setEnabled(true);
    } else {
        page->setVisible(false);
        page->setEnabled(false);
    }

    return d->tabBar->insertTab(index, icon, label);
}

int TabBar::insertTab(int index, const QString &label, QGraphicsLayoutItem *content)
{
    return insertTab(index, QIcon(), label, content);
}

int TabBar::addTab(const QIcon &icon, const QString &label, QGraphicsLayoutItem *content)
{
    return insertTab(d->pages.count(), icon, label, content);
}

int TabBar::addTab(const QString &label, QGraphicsLayoutItem *content)
{
    return insertTab(d->pages.count(), QIcon(), label, content);
}

int TabBar::currentIndex() const
{
    return d->tabBar->currentIndex();
}

void TabBar::setCurrentIndex(int index)
{
    if (index > d->tabBar->count() || d->tabBar->count() <= 1) {
        return;
    }

    if (d->currentIndex != index) {
        d->tabBar->setCurrentIndex(index);
    }

    d->mainLayout->removeAt(1);

    d->oldPage = d->pages[d->currentIndex];
    d->newPage = d->pages[index];

//FIXME: this part should be enabled again once
//http://trolltech.com/developer/task-tracker/index_html?id=220488
//get fixed
#ifdef USE_SLIDING_ANIMATION
    d->newPage->resize(d->oldPage->size());

    setFlags(QGraphicsItem::ItemClipsChildrenToShape);

    //if an animation was in rogress hide everything to avoid an inconsistent state
    if (d->newPageAnimId != -1 || d->oldPageAnimId != -1) {
        foreach (QGraphicsWidget *page, d->pages) {
            page->hide();
        }
        if (d->newPageAnimId != -1) {
            Animator::self()->stopItemMovement(d->newPageAnimId);
        }    
        if (d->oldPageAnimId != -1) {
            Animator::self()->stopItemMovement(d->oldPageAnimId);
        }
    }

    d->oldPage->show();
    d->newPage->show();
    d->newPage->setEnabled(true);

    d->oldPage->setEnabled(false);

    QRect beforeCurrentGeom(d->oldPage->geometry().toRect());
    beforeCurrentGeom.moveTopRight(beforeCurrentGeom.topLeft());

    d->newPageAnimId = Animator::self()->moveItem(d->newPage, Plasma::Animator::SlideOutMovement, d->oldPage->pos().toPoint());
    if (index > d->currentIndex) {
        d->newPage->setPos(d->oldPage->geometry().topRight());
        d->oldPageAnimId = Animator::self()->moveItem(d->oldPage, Plasma::Animator::SlideOutMovement, beforeCurrentGeom.topLeft());
    } else {
        d->newPage->setPos(beforeCurrentGeom.topLeft());
        d->oldPageAnimId = Animator::self()->moveItem(d->oldPage, Plasma::Animator::SlideOutMovement, d->oldPage->geometry().topRight().toPoint());
    }
#else
    d->mainLayout->addItem(d->pages[index]);
    d->oldPage->hide();
    d->newPage->show();
    d->newPage->setEnabled(true);
    d->oldPage->setEnabled(false);
#endif
    d->currentIndex = index;
    emit currentChanged(index);
}

int TabBar::count() const
{
    return d->pages.count();
}

void TabBar::removeTab(int index)
{
    if (index > d->pages.count()) {
        return;
    }

    int currentIndex = d->tabBar->currentIndex();

    d->tabBar->removeTab(index);
    QGraphicsWidget *page = d->pages.takeAt(index);

    if (index == currentIndex) {
        setCurrentIndex(currentIndex);
    }

    scene()->removeItem(page);
    page->deleteLater();
}

void TabBar::setTabText(int index, const QString &label)
{
    if (index > d->pages.count()) {
        return;
    }

    d->tabBar->setTabText(index, label);
}

QString TabBar::tabText(int index) const
{
    return d->tabBar->tabText(index);
}

void TabBar::setTabIcon(int index, const QIcon &icon)
{
    d->tabBar->setTabIcon(index, icon);
}

QIcon TabBar::tabIcon(int index) const
{
    return d->tabBar->tabIcon(index);
}

void TabBar::setStyleSheet(const QString &stylesheet)
{
    d->tabBar->setStyleSheet(stylesheet);
}

QString TabBar::styleSheet() const
{
    return d->tabBar->styleSheet();
}

QTabBar *TabBar::nativeWidget() const
{
    return d->tabBar;
}

void TabBar::paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    d->background->paintPanel(painter, QPoint(contentsRect().left(), contentsRect().top() + nativeWidget()->height()/1.5));
}

void TabBar::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    d->background->resizePanel(event->newSize() - QSize(0, nativeWidget()->height()/2));
}

void TabBar::wheelEvent(QGraphicsSceneWheelEvent * event)
{
    //FIXME: probably this would make more sense in NativeTabBar, but it works only here

    if (d->tabBar->underMouse()) {
        //Cycle tabs with the circular array tecnique
        if (event->delta() < 0) {
            int index = d->tabBar->currentIndex();
            //search for an enabled tab
            for (int i = 0; i < d->tabBar->count()-1; ++i) {
                index = (index + 1) % d->tabBar->count();
                if (d->tabBar->isTabEnabled(index)) {
                    break;
                }
            }

            d->tabBar->setCurrentIndex(index);
        } else {
            int index = d->tabBar->currentIndex();
            for (int i = 0; i < d->tabBar->count()-1; ++i) {
                index = (d->tabBar->count() + index -1) % d->tabBar->count();
                if (d->tabBar->isTabEnabled(index)) {
                    break;
                }
            }

            d->tabBar->setCurrentIndex(index);
        }
    } else {
        QGraphicsWidget::wheelEvent(event);
    }
}

} // namespace Plasma

#include <tabbar.moc>

