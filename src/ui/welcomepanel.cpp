#include "welcomepanel.h"

#include "playback/mediasource.h"

#include <QAction>
#include <QBoxLayout>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

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
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMinimumWidth(224);
    setMaximumWidth(460);

    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetNoConstraint);
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

    auto *recentHeading = new QLabel(this);
    recentHeading->setObjectName(QStringLiteral("welcomeRecentHeading"));
    recentHeading->setVisible(false);
    recentHeading->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    recentHeading->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(recentHeading);
    recentLayout_ = new QVBoxLayout;
    recentLayout_->setContentsMargins(0, 0, 0, 0);
    recentLayout_->setSpacing(4);
    layout->addLayout(recentLayout_);

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
    updateRecentButtonLabels();
    updateVerticalDensity();
}

void WelcomePanel::updateButtonDirection() {
    const auto *openButton = findChild<QPushButton *>(QStringLiteral("welcomeOpenButton"));
    const auto *networkButton = findChild<QPushButton *>(QStringLiteral("welcomeNetworkButton"));
    if (!openButton || !networkButton)
        return;

    constexpr int horizontalSpacing = 8;
    constexpr int horizontalMargins = 56;
    const auto requiredWidth = openButton->sizeHint().width() + networkButton->sizeHint().width() +
                               horizontalSpacing + horizontalMargins;
    const auto direction =
        width() >= requiredWidth ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom;
    if (buttonsLayout_->direction() != direction) {
        buttonsLayout_->setDirection(direction);
        buttonsLayout_->invalidate();
        buttonsLayout_->activate();
    }
}

void WelcomePanel::setRecentSources(const QStringList &sources) {
    recentSources_ = sources.mid(0, 3);
    while (auto *item = recentLayout_->takeAt(0)) {
        if (item->widget())
            delete item->widget();
        delete item;
    }
    auto *heading = findChild<QLabel *>(QStringLiteral("welcomeRecentHeading"));
    heading->setVisible(!recentSources_.isEmpty());

    for (qsizetype index = 0; index < recentSources_.size(); ++index) {
        const auto source = recentSources_.at(index);
        auto *button = new QPushButton(this);
        button->setObjectName(QStringLiteral("welcomeRecentAction%1").arg(index + 1));
        button->setProperty("welcomeRecentAction", true);
        button->setMinimumHeight(34);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const auto networkSource = MediaSource::isNetwork(source);
        const auto location =
            networkSource ? QUrl(source).toDisplayString(QUrl::RemoveUserInfo | QUrl::RemoveQuery |
                                                         QUrl::RemoveFragment)
                          : source;
        button->setIcon(
            style()->standardIcon(networkSource ? QStyle::SP_DriveNetIcon : QStyle::SP_FileIcon));
        button->setIconSize(QSize(16, 16));
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("welcomeRecentNetwork", networkSource);
        button->setToolTip(location);
        button->setAccessibleName(
            tr(networkSource ? "网络媒体：%1" : "本地媒体：%1").arg(location));
        connect(button, &QPushButton::clicked, this,
                [this, source] { emit recentSourceRequested(source); });
        recentLayout_->addWidget(button);
    }
    updateRecentButtonLabels();
    updateVerticalDensity();
}

void WelcomePanel::updateRecentButtonLabels() {
    const auto availableWidth = std::max(0, width() - 80);
    const QFontMetrics metrics(font());
    for (qsizetype index = 0; index < recentSources_.size(); ++index) {
        auto *button =
            findChild<QPushButton *>(QStringLiteral("welcomeRecentAction%1").arg(index + 1));
        auto label = MediaSource::displayName(recentSources_.at(index));
        if (label.isEmpty())
            label = tr("未命名媒体");
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));
        button->setText(metrics.elidedText(label, Qt::ElideMiddle, availableWidth));
        const auto networkSource = button->property("welcomeRecentNetwork").toBool();
        const auto location = networkSource
                                  ? QUrl(recentSources_.at(index))
                                        .toDisplayString(QUrl::RemoveUserInfo | QUrl::RemoveQuery |
                                                         QUrl::RemoveFragment)
                                  : recentSources_.at(index);
        button->setAccessibleName(
            tr(networkSource ? "网络媒体：%1" : "本地媒体：%1").arg(location));
    }
}

void WelcomePanel::updateVerticalDensity() {
    const bool showRecent = height() >= 260;
    const bool showSupportingText = height() >= 215;
    if (auto *heading = findChild<QLabel *>(QStringLiteral("welcomeRecentHeading")))
        heading->setVisible(showRecent && !recentSources_.isEmpty());
    for (qsizetype index = 0; index < recentSources_.size(); ++index) {
        if (auto *button =
                findChild<QPushButton *>(QStringLiteral("welcomeRecentAction%1").arg(index + 1)))
            button->setVisible(showRecent);
    }
    if (auto *subtitle = findChild<QLabel *>(QStringLiteral("welcomeSubtitle")))
        subtitle->setVisible(showSupportingText);
    if (auto *hint = findChild<QLabel *>(QStringLiteral("welcomeDropHint")))
        hint->setVisible(showSupportingText);
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
    if (auto *heading = findChild<QLabel *>(QStringLiteral("welcomeRecentHeading"))) {
        heading->setText(tr("最近播放"));
        updateVerticalDensity();
    }
    updateRecentButtonLabels();
}
