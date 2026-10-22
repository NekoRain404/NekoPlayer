#include "actionfeedbackoverlay.h"
#include "actionfeedbackgeometry.h"

#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>

ActionFeedbackOverlay::ActionFeedbackOverlay(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("actionFeedbackOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, &ActionFeedbackOverlay::clearMessage);
    hide();
}

void ActionFeedbackOverlay::showMessage(const QString &message, int durationMs) {
    message_ = message.trimmed();
    setAccessibleDescription(message_);
    if (message_.isEmpty()) {
        clearMessage();
        return;
    }
    timer_.start(qMax(1, durationMs));
    show();
    raise();
    update();
}

void ActionFeedbackOverlay::clearMessage() {
    timer_.stop();
    if (message_.isEmpty() && !isVisible())
        return;
    message_.clear();
    setAccessibleDescription(QString());
    hide();
    update();
}

void ActionFeedbackOverlay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    if (message_.isEmpty())
        return;

    QFont font = this->font();
    font.setPixelSize(18);
    const QFontMetrics metrics(font);
    const int maxTextWidth = qMax(1, width() - 64);
    const QRect textBounds = metrics.boundingRect(QRect(0, 0, maxTextWidth, height()),
                                                  Qt::AlignCenter | Qt::TextWordWrap, message_);
    const auto layout = ActionFeedbackGeometry::arrange(size(), textBounds.size());
    if (!layout.panel.isValid())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(24, 26, 31, 218));
    painter.drawRoundedRect(layout.panel, 6, 6);
    painter.setPen(QColor(245, 246, 249));
    painter.drawText(layout.text, Qt::AlignCenter | Qt::TextWordWrap, message_);
}
