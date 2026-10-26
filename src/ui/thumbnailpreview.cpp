#include "thumbnailpreview.h"
#include "appearancetokens.h"
#include "uimotionduration.h"

#include <QGuiApplication>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QShowEvent>
#include <QStyleHints>
#include <algorithm>

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
#include <QAccessibilityHints>
#endif

namespace {
QString timestampText(double seconds) {
    const auto total = static_cast<qint64>(std::max(0.0, seconds));
    return QStringLiteral("%1:%2:%3")
        .arg(total / 3600, 2, 10, QLatin1Char('0'))
        .arg((total / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}
} // namespace

namespace {
int currentMotionDuration() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    const auto preference = QGuiApplication::styleHints()->accessibility()->motionPreference();
    return uiMotionDuration(appearanceTokens(AppearancePreferences::Theme::Dark).motionDurationMs,
                            preference == Qt::MotionPreference::ReducedMotion);
#else
    return uiMotionDuration(appearanceTokens(AppearancePreferences::Theme::Dark).motionDurationMs,
                            false);
#endif
}
} // namespace

ThumbnailPreview::ThumbnailPreview(QWidget *parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
      animation_(new QPropertyAnimation(this, QByteArrayLiteral("contentOpacity"), this)) {
    setObjectName(QStringLiteral("thumbnailPreview"));
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(false);
    animation_->setEasingCurve(uiMotionEasingCurve());
    animation_->setDuration(currentMotionDuration());
    connect(animation_, &QPropertyAnimation::finished, this, [this] {
        if (contentOpacity_ == 0.0) {
            image_ = {};
            QWidget::hide();
        }
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    connect(QGuiApplication::styleHints()->accessibility(),
            &QAccessibilityHints::motionPreferenceChanged, this,
            [this](Qt::MotionPreference preference) {
                if (preference != Qt::MotionPreference::ReducedMotion ||
                    animation_->state() != QAbstractAnimation::Running)
                    return;
                const qreal target = animation_->endValue().toReal();
                animation_->stop();
                animateContentTo(target);
            });
#endif
}

void ThumbnailPreview::present(const QImage &image, double seconds) {
    image_ = image;
    seconds_ = seconds;
    setFixedSize(sizeHint());
    update();
}

void ThumbnailPreview::clear() {
    animation_->stop();
    if (!isVisible()) {
        image_ = {};
        return;
    }
    animateContentTo(0.0);
}

QSize ThumbnailPreview::sizeHint() const { return QSize(248, 166); }

qreal ThumbnailPreview::contentOpacity() const { return contentOpacity_; }

void ThumbnailPreview::setContentOpacity(qreal opacity) {
    contentOpacity_ = qBound(0.0, opacity, 1.0);
    update();
}

void ThumbnailPreview::animateContentTo(qreal opacity) {
    animation_->stop();
    animation_->setDuration(currentMotionDuration());
    if (animation_->duration() == 0 || qFuzzyCompare(contentOpacity_ + 1, opacity + 1)) {
        setContentOpacity(opacity);
        if (opacity == 0.0) {
            image_ = {};
            QWidget::hide();
        }
        return;
    }
    animation_->setStartValue(contentOpacity_);
    animation_->setEndValue(opacity);
    animation_->start();
}

void ThumbnailPreview::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (contentOpacity_ < 1.0)
        animateContentTo(1.0);
}

void ThumbnailPreview::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setOpacity(contentOpacity_);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto frame = rect().adjusted(1, 1, -1, -1);
    const auto panel = palette().color(QPalette::Window);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(panel);
    painter.drawRoundedRect(frame, 10, 10);

    const auto imageRect = frame.adjusted(8, 8, -8, -28);
    if (image_.isNull()) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(imageRect, Qt::AlignCenter, tr("预览不可用"));
    } else {
        painter.setClipRect(imageRect);
        painter.drawImage(imageRect, image_, image_.rect());
        painter.setClipping(false);
    }

    const auto badge = QRect(frame.left() + 8, frame.bottom() - 25, frame.width() - 16, 17);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 165));
    painter.drawRoundedRect(badge, 5, 5);
    painter.setPen(Qt::white);
    painter.drawText(badge, Qt::AlignCenter, timestampText(seconds_));
}
