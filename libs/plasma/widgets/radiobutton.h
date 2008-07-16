/*
 *   Copyright 2008 Aaron Seigo <aseigo@kde.org>
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


#ifndef PLASMA_RADIOBUTTON_H
#define PLASMA_RADIOBUTTON_H

#include <QtGui/QGraphicsProxyWidget>

class QRadioButton;

#include <plasma/plasma_export.h>

namespace Plasma
{

class RadioButtonPrivate;

class PLASMA_EXPORT RadioButton : public QGraphicsProxyWidget
{
    Q_OBJECT

    Q_PROPERTY(QGraphicsWidget* parentWidget READ parentWidget)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QString image READ image WRITE setImage)
    Q_PROPERTY(QString stylesheet READ styleSheet WRITE setStyleSheet)
    Q_PROPERTY(QRadioButton* nativeWidget READ nativeWidget)
    Q_PROPERTY(bool isChecked READ isChecked WRITE setChecked)

public:
    explicit RadioButton(QGraphicsWidget *parent = 0);
    ~RadioButton();

    /**
     * Sets the display text for this RadioButton
     * 
     * @arg text the text to display; should be translated.
     */
    void setText(const QString &text);

    /**
     * @return the display text
     */
    QString text() const;

    /**
     * Sets the path to an image to display.
     *
     * @arg path the path to the image; if a relative path, then a themed image will be loaded.
     */
    void setImage(const QString &path);

    /**
     * @return the image path being displayed currently, or an empty string if none.
     */
    QString image() const;

    /**
     * Sets the stylesheet used to control the visual display of this RadioButton
     *
     * @arg stylesheet a CSS string
     */
    void setStyleSheet(const QString &stylesheet);

    /**
     * @return the stylesheet currently used with this widget
     */
    QString styleSheet();

    /**
     * @return the native widget wrapped by this RadioButton
     */
    QRadioButton* nativeWidget() const;

    /**
     * Sets the checked state.
     *
     * @arg checked true if checked, false if not
     */
    void setChecked(bool checked);

    /**
     * @return the checked state
     */
    bool isChecked() const;

Q_SIGNALS:
    void toggled(bool);

protected:
    void resizeEvent(QGraphicsSceneResizeEvent *event);

private:
    RadioButtonPrivate * const d;
};

} // namespace Plasma

#endif // multiple inclusion guard
