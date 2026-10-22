#include "welcomepanel.h"

#include <QAction>
#include <QBoxLayout>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {
void bindButton(QPushButton *button, QAction *action) {
    button->setText(action->text());
    button->setIcon(action->icon());
    button->setEnabled(action->isEnabled());
    button->setAccessibleName(action->text());
    button->setToolTip(action->text());
    QObject::connect(button, &QPushButton::clicked, action, &QAction::trigger);
    QObject::connect(action, &QAction::changed, button, [button, action] {
        button->setText(action->text());
        button->setIcon(action->icon());
        button->setEnabled(action->isEnabled());
        button->setAccessibleName(action->text());
        button->setToolTip(action->text());
    });
}
} // namespace

WelcomePanel::WelcomePanel(QAction *open, QAction *network, QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("welcomePanel"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMinimumWidth(224);
    setMaximumWidth(460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 22);
    layout->setSpacing(8);

    auto *title = new QLabel(this);
    title->setObjectName(QStringLiteral("welcomeTitle"));
    title->setAlignment(Qt::AlignCenter);
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(title);

    auto *subtitle = new QLabel(this);
    subtitle->setObjectName(QStringLiteral("welcomeSubtitle"));
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);
    subtitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(subtitle);

    buttonsLayout_ = new QHBoxLayout;
    buttonsLayout_->setContentsMargins(0, 10, 0, 4);
    buttonsLayout_->setSpacing(8);
    auto *openButton = new QPushButton(this);
    openButton->setObjectName(QStringLiteral("welcomeOpenButton"));
    bindButton(openButton, open);
    openButton->setMinimumHeight(38);
    openButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *networkButton = new QPushButton(this);
    networkButton->setObjectName(QStringLiteral("welcomeNetworkButton"));
    bindButton(networkButton, network);
    networkButton->setMinimumHeight(38);
    networkButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    buttonsLayout_->addWidget(openButton);
    buttonsLayout_->addWidget(networkButton);
    layout->addLayout(buttonsLayout_);

    auto *hint = new QLabel(this);
    hint->setObjectName(QStringLiteral("welcomeDropHint"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    hint->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(hint);
    updateButtonDirection();
    retranslateUi();
}

void WelcomePanel::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void WelcomePanel::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateButtonDirection();
}

void WelcomePanel::updateButtonDirection() {
    const auto buttons = findChildren<QPushButton *>();
    if (buttons.size() < 2)
        return;

    constexpr int horizontalSpacing = 8;
    constexpr int horizontalMargins = 56;
    const auto requiredWidth = buttons[0]->sizeHint().width() + buttons[1]->sizeHint().width() +
                               horizontalSpacing + horizontalMargins;
    const auto direction =
        width() >= requiredWidth ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom;
    if (buttonsLayout_->direction() != direction) {
        buttonsLayout_->setDirection(direction);
        buttonsLayout_->invalidate();
        buttonsLayout_->activate();
    }
}

void WelcomePanel::retranslateUi() {
    if (auto *title = findChild<QLabel *>(QStringLiteral("welcomeTitle")))
        title->setText(tr("开始播放"));
    if (auto *subtitle = findChild<QLabel *>(QStringLiteral("welcomeSubtitle")))
        subtitle->setText(tr("打开本地媒体或连接网络地址，享受流畅播放体验"));
    if (auto *hint = findChild<QLabel *>(QStringLiteral("welcomeDropHint"))) {
        hint->setText(tr("也可以将视频拖到这里 · Ctrl+O 打开文件 · Ctrl+U 打开网络地址"));
        hint->setAccessibleName(hint->text());
    }
}
