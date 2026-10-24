#include "welcomepanel.h"
#include "appearanceicons.h"

#include "playback/mediasource.h"
#include "playback/playbacktime.h"

#include <QAction>
#include <QBoxLayout>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
QString safeLocation(const QString &source, bool networkSource) {
    if (!networkSource)
        return source;
    return QUrl(source).toDisplayString(QUrl::RemoveUserInfo | QUrl::RemoveQuery |
                                        QUrl::RemoveFragment);
}

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
    setMinimumWidth(236);
    setMaximumWidth(500);

    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    layout->setContentsMargins(30, 24, 30, 22);
    layout->setSpacing(7);

    auto *brand = new QLabel(QStringLiteral("NEKOPLAYER"), this);
    brand->setObjectName(QStringLiteral("welcomeBrand"));
    brand->setAlignment(Qt::AlignCenter);
    brand->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(brand);

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
    buttonsLayout_->setContentsMargins(0, 14, 0, 10);
    buttonsLayout_->setSpacing(10);
    auto *openButton = new QPushButton(this);
    openButton->setObjectName(QStringLiteral("welcomeOpenButton"));
    bindButton(openButton, open);
    openButton->setMinimumHeight(44);
    openButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *networkButton = new QPushButton(this);
    networkButton->setObjectName(QStringLiteral("welcomeNetworkButton"));
    bindButton(networkButton, network);
    networkButton->setMinimumHeight(44);
    networkButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    buttonsLayout_->addWidget(openButton);
    buttonsLayout_->addWidget(networkButton);
    layout->addLayout(buttonsLayout_);

    auto *continueHeading = new QLabel(this);
    continueHeading->setObjectName(QStringLiteral("welcomeContinueHeading"));
    continueHeading->setVisible(false);
    continueHeading->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    continueHeading->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(continueHeading);
    continueLayout_ = new QVBoxLayout;
    continueLayout_->setContentsMargins(0, 0, 0, 0);
    continueLayout_->setSpacing(6);
    layout->addLayout(continueLayout_);

    auto *recentHeading = new QLabel(this);
    recentHeading->setObjectName(QStringLiteral("welcomeRecentHeading"));
    recentHeading->setVisible(false);
    recentHeading->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    recentHeading->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(recentHeading);
    recentLayout_ = new QVBoxLayout;
    recentLayout_->setContentsMargins(0, 0, 0, 0);
    recentLayout_->setSpacing(7);
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
    if (event->type() == QEvent::PaletteChange) {
        for (auto *label : findChildren<QLabel *>()) {
            if (!label->property("welcomeMediaIcon").toBool())
                continue;
            const auto network = label->parentWidget()->property("welcomeRecentNetwork").toBool();
            const auto icon = AppearanceIcons::icon(network ? AppearanceIcons::Symbol::Network
                                                            : AppearanceIcons::Symbol::MediaFile,
                                                    palette());
            label->setPixmap(icon.pixmap(18, 18));
        }
    }
}

void WelcomePanel::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath surface;
    surface.addRoundedRect(rect().adjusted(0, 0, -1, -1), 14, 14);
    painter.fillPath(surface, palette().color(QPalette::Window));
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
        button->setMinimumHeight(60);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const auto networkSource = MediaSource::isNetwork(source);
        const auto location = safeLocation(source, networkSource);
        auto *rowLayout = new QHBoxLayout(button);
        rowLayout->setContentsMargins(12, 8, 12, 8);
        rowLayout->setSpacing(12);

        auto *icon = new QLabel(button);
        icon->setObjectName(QStringLiteral("welcomeRecentMediaIcon%1").arg(index + 1));
        icon->setProperty("welcomeMediaIcon", true);
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        icon->setAlignment(Qt::AlignCenter);
        icon->setFixedSize(30, 30);
        const auto mediaIcon = AppearanceIcons::icon(
            networkSource ? AppearanceIcons::Symbol::Network : AppearanceIcons::Symbol::MediaFile,
            button->palette());
        icon->setPixmap(mediaIcon.pixmap(18, 18));
        rowLayout->addWidget(icon, 0, Qt::AlignVCenter);

        auto *textLayout = new QVBoxLayout;
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(3);
        auto *title = new QLabel(button);
        title->setObjectName(QStringLiteral("welcomeRecentTitle%1").arg(index + 1));
        title->setProperty("welcomeRecentTitle", true);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *locationLabel = new QLabel(button);
        locationLabel->setObjectName(QStringLiteral("welcomeRecentLocation%1").arg(index + 1));
        locationLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        locationLabel->setProperty("welcomeRecentLocation", true);
        title->setText(MediaSource::displayName(source));
        locationLabel->setText(location);
        textLayout->addWidget(title);
        textLayout->addWidget(locationLabel);
        rowLayout->addLayout(textLayout, 1);

        auto *chevron = new QLabel(QStringLiteral("›"), button);
        chevron->setObjectName(QStringLiteral("welcomeRecentChevron"));
        chevron->setAttribute(Qt::WA_TransparentForMouseEvents);
        chevron->setAlignment(Qt::AlignCenter);
        rowLayout->addWidget(chevron, 0, Qt::AlignVCenter);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setProperty("welcomeRecentNetwork", networkSource);
        button->setToolTip(location);
        button->setAccessibleName(
            tr(networkSource ? "网络媒体：%1" : "本地媒体：%1")
                .arg(MediaSource::displayName(source) + QStringLiteral("，") + location));
        connect(button, &QPushButton::clicked, this,
                [this, source] { emit recentSourceRequested(source); });
        recentLayout_->addWidget(button);
    }
    updateRecentButtonLabels();
    updateVerticalDensity();
}

void WelcomePanel::setResumeEntries(const QList<PlaybackHistoryEntry> &entries) {
    resumeEntries_.clear();
    for (const auto &entry : entries) {
        if (PlaybackHistoryStore::normalizedPath(entry.path).isEmpty() ||
            !std::isfinite(entry.position) || !std::isfinite(entry.duration) ||
            entry.position <= 0 || entry.duration <= 0 || entry.position >= entry.duration ||
            entry.duration - entry.position <= 1.0)
            continue;
        resumeEntries_.append(entry);
        if (resumeEntries_.size() == 3)
            break;
    }
    while (auto *item = continueLayout_->takeAt(0)) {
        if (item->widget())
            delete item->widget();
        delete item;
    }
    auto *heading = findChild<QLabel *>(QStringLiteral("welcomeContinueHeading"));
    heading->setVisible(!resumeEntries_.isEmpty());
    for (qsizetype index = 0; index < resumeEntries_.size(); ++index) {
        const auto entry = resumeEntries_.at(index);
        auto *button = new QPushButton(this);
        button->setObjectName(QStringLiteral("welcomeContinueAction%1").arg(index + 1));
        button->setProperty("welcomeContinueAction", true);
        button->setMinimumHeight(48);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *row = new QVBoxLayout(button);
        row->setContentsMargins(12, 6, 12, 6);
        row->setSpacing(3);
        auto *title = new QLabel(MediaSource::displayName(entry.path), button);
        title->setObjectName(QStringLiteral("welcomeContinueTitle%1").arg(index + 1));
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->addWidget(title);
        auto *progress = new QProgressBar(button);
        progress->setObjectName(QStringLiteral("welcomeContinueProgress%1").arg(index + 1));
        progress->setRange(0, 1000);
        const auto fraction = std::clamp(entry.position / entry.duration, 0.0, 1.0);
        progress->setValue(qRound(fraction * 1000.0));
        progress->setTextVisible(true);
        progress->setFormat(tr("%1 / %2").arg(formatPlaybackTime(entry.position),
                                              formatPlaybackTime(entry.duration)));
        progress->setFixedHeight(16);
        row->addWidget(progress);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setAccessibleName(tr("继续观看：%1，%2 / %3")
                                      .arg(MediaSource::displayName(entry.path),
                                           formatPlaybackTime(entry.position),
                                           formatPlaybackTime(entry.duration)));
        connect(button, &QPushButton::clicked, this,
                [this, path = entry.path] { emit resumeSourceRequested(path); });
        continueLayout_->addWidget(button);
    }
    updateVerticalDensity();
}

void WelcomePanel::updateRecentButtonLabels() {
    const auto availableWidth = std::max(0, width() - 156);
    const QFontMetrics metrics(font());
    for (qsizetype index = 0; index < recentSources_.size(); ++index) {
        const auto suffix = QString::number(index + 1);
        auto *button =
            findChild<QPushButton *>(QStringLiteral("welcomeRecentAction%1").arg(suffix));
        auto *title = findChild<QLabel *>(QStringLiteral("welcomeRecentTitle%1").arg(suffix));
        auto *locationLabel =
            findChild<QLabel *>(QStringLiteral("welcomeRecentLocation%1").arg(suffix));
        if (!button || !title || !locationLabel)
            continue;
        auto label = MediaSource::displayName(recentSources_.at(index));
        if (label.isEmpty())
            label = tr("未命名媒体");
        const auto networkSource = button->property("welcomeRecentNetwork").toBool();
        const auto location = safeLocation(recentSources_.at(index), networkSource);
        title->setText(metrics.elidedText(label, Qt::ElideMiddle, availableWidth));
        locationLabel->setText(metrics.elidedText(location, Qt::ElideMiddle, availableWidth));
        button->setAccessibleName(tr(networkSource ? "网络媒体：%1" : "本地媒体：%1")
                                      .arg(label + QStringLiteral("，") + location));
    }
}

void WelcomePanel::updateVerticalDensity() {
    const bool showRecent = height() >= 260;
    const bool showSupportingText = height() >= 260;
    const bool showBrand = height() >= 165;
    const bool showContinue = height() >= 360;
    if (auto *brand = findChild<QLabel *>(QStringLiteral("welcomeBrand")))
        brand->setVisible(showBrand);
    if (auto *heading = findChild<QLabel *>(QStringLiteral("welcomeRecentHeading")))
        heading->setVisible(showRecent && !recentSources_.isEmpty());
    if (auto *heading = findChild<QLabel *>(QStringLiteral("welcomeContinueHeading")))
        heading->setVisible(showContinue && !resumeEntries_.isEmpty());
    for (qsizetype index = 0; index < resumeEntries_.size(); ++index) {
        if (auto *button =
                findChild<QPushButton *>(QStringLiteral("welcomeContinueAction%1").arg(index + 1)))
            button->setVisible(showContinue);
    }
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
    if (auto *heading = findChild<QLabel *>(QStringLiteral("welcomeContinueHeading")))
        heading->setText(tr("继续观看"));
    updateRecentButtonLabels();
}
