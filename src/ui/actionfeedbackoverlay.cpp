#include "actionfeedbackoverlay.h"

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
    const int maxTextWidth = qMax(80, width() - 64);
    const QRect textBounds = metrics.boundingRect(QRect(0, 0, maxTextWidth, height()),
                                                  Qt::AlignCenter | Qt::TextWordWrap, message_);
    const QSize panelSize(textBounds.width() + 36, textBounds.height() + 24);
    const QPoint panelTopLeft((width() - panelSize.width()) / 2,
                              qBound(16, qRound(height() * 0.68) - panelSize.height() / 2,
                                     qMax(16, height() - panelSize.height() - 16)));
    const QRect panel(panelTopLeft, panelSize);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(24, 26, 31, 218));
    painter.drawRoundedRect(panel, 6, 6);
    painter.setPen(QColor(245, 246, 249));
    painter.drawText(panel.adjusted(18, 10, -18, -10), Qt::AlignCenter | Qt::TextWordWrap,
                     message_);
}
