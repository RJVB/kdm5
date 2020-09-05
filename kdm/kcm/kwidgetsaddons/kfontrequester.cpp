/*
    SPDX-FileCopyrightText: 2003 Nadeem Hasan <nhasan@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfontrequester.h"
#include "fonthelpers_p.h"

#include "kfontchooserdialog.h"

#include <QLabel>
#include <QPushButton>
#include <QFontDatabase>
#include <QFontInfo>
#include <QHBoxLayout>

#include <cmath>

// Determine if the font with given properties is available on the system,
// otherwise find and return the best fitting combination.
static QFont nearestExistingFont(const QFont &font)
{
    QFontDatabase dbase;

    // Initialize font data according to given font object.
    QString family = font.family();
    QString style = dbase.styleString(font);
    qreal size = font.pointSizeF();

    // Check if the family exists.
    const QStringList families = dbase.families();
    if (!families.contains(family)) {
        // Chose another family.
        family = QFontInfo(font).family(); // the nearest match
        if (!families.contains(family)) {
            family = families.count() ? families.at(0) : QStringLiteral("fixed");
        }
    }

    // Check if the family has the requested style.
    // Easiest by piping it through font selection in the database.
    QString retStyle = dbase.styleString(dbase.font(family, style, 10));
    style = retStyle;

    // Check if the family has the requested size.
    // Only for bitmap fonts.
    if (!dbase.isSmoothlyScalable(family, style)) {
        const QList<int> sizes = dbase.smoothSizes(family, style);
        if (!sizes.contains(size)) {
            // Find nearest available size.
            int mindiff = 1000;
            int refsize = size;
            for (int lsize : sizes) {
                int diff = qAbs(refsize - lsize);
                if (mindiff > diff) {
                    mindiff = diff;
                    size = lsize;
                }
            }
        }
    }

    // Select the font with confirmed properties.
    QFont result = dbase.font(family, style, int(size));
    if (dbase.isSmoothlyScalable(family, style) && result.pointSize() == floor(size)) {
        result.setPointSizeF(size);
    }
    return result;
}

class Q_DECL_HIDDEN KFontRequester::KFontRequesterPrivate
{
public:
    KFontRequesterPrivate(KFontRequester *q): q(q) {}

    void displaySampleText();
    void setToolTip();

    void _k_buttonClicked();

    KFontRequester *q;
    bool m_onlyFixed;
    QString m_sampleText, m_title;
    QLabel *m_sampleLabel = nullptr;
    QPushButton *m_button = nullptr;
    QFont m_selFont;
    bool m_alwaysTriggerSignal = true;
};

KFontRequester::KFontRequester(QWidget *parent, bool onlyFixed)
    : QWidget(parent), d(new KFontRequesterPrivate(this))
{
    d->m_onlyFixed = onlyFixed;

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    d->m_sampleLabel = new QLabel(this);
    d->m_button = new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), QString(), this);

    d->m_sampleLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setFocusProxy(d->m_button);
    setFocusPolicy(d->m_button->focusPolicy());

    layout->addWidget(d->m_sampleLabel, 1);
    layout->addWidget(d->m_button);

    connect(d->m_button, &QPushButton::clicked, this, [this] { d->_k_buttonClicked(); });

    d->displaySampleText();
    d->setToolTip();
}

KFontRequester::~KFontRequester()
{
    delete d;
}

QFont KFontRequester::font() const
{
    return d->m_selFont;
}

bool KFontRequester::isFixedOnly() const
{
    return d->m_onlyFixed;
}

QString KFontRequester::sampleText() const
{
    return d->m_sampleText;
}

QString KFontRequester::title() const
{
    return d->m_title;
}

QLabel *KFontRequester::label() const
{
    return d->m_sampleLabel;
}

QPushButton *KFontRequester::button() const
{
    return d->m_button;
}

void KFontRequester::setFont(const QFont &font, bool onlyFixed)
{
    d->m_selFont = nearestExistingFont(font);
    d->m_onlyFixed = onlyFixed;

    d->displaySampleText();
    if (d->m_alwaysTriggerSignal) {
        emit fontSelected(d->m_selFont);
    }
}

void KFontRequester::setSampleText(const QString &text)
{
    d->m_sampleText = text;
    d->displaySampleText();
}

void KFontRequester::setTitle(const QString &title)
{
    d->m_title = title;
    d->setToolTip();
}

bool KFontRequester::alwaysTriggerSignal()
{
    return d->m_alwaysTriggerSignal;
}

void KFontRequester::setAlwaysTriggerSignal(bool enable)
{
    d->m_alwaysTriggerSignal = enable;
}

void KFontRequester::KFontRequesterPrivate::_k_buttonClicked()
{
    KFontChooser::DisplayFlags flags = m_onlyFixed ? KFontChooser::FixedFontsOnly
                                                     : KFontChooser::NoDisplayFlags;

    const int result = KFontChooserDialog::getFont(m_selFont, flags, q->parentWidget());

    if (result == QDialog::Accepted) {
        displaySampleText();
        emit q->fontSelected(m_selFont);
    }
}

void KFontRequester::KFontRequesterPrivate::displaySampleText()
{
    m_sampleLabel->setFont(m_selFont);

    qreal size = m_selFont.pointSizeF();
    if (size == -1) {
        size = m_selFont.pixelSize();
    }

    if (m_sampleText.isEmpty()) {
        QString family = translateFontName(m_selFont.family());
        m_sampleLabel->setText(QStringLiteral("%1 %2").arg(family).arg(size));
    } else {
        m_sampleLabel->setText(m_sampleText);
    }
}

void KFontRequester::KFontRequesterPrivate::setToolTip()
{
    m_button->setToolTip(tr("Choose font...", "@info:tooltip"));

    m_sampleLabel->setToolTip(QString());
    m_sampleLabel->setWhatsThis(QString());

    if (m_title.isNull()) {
        m_sampleLabel->setToolTip(tr("Preview of the selected font", "@info:tooltip"));
        m_sampleLabel->setWhatsThis(tr("This is a preview of the selected font. You can change it"
                                       " by clicking the \"Choose Font...\" button.", "@info:whatsthis"));
    } else {
        m_sampleLabel->setToolTip(tr("Preview of the \"%1\" font", "@info:tooltip").arg(m_title));
        m_sampleLabel->setWhatsThis(tr("This is a preview of the \"%1\" font. You can change it"
                                       " by clicking the \"Choose Font...\" button.", "@info:whatsthis").arg(m_title));
    }
}

#include "moc_kfontrequester.cpp"

