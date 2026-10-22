#include "recentmediamenu.h"
#include "playback/mediasource.h"
#include "session/recentmediahistory.h"

#include <QAction>
#include <QFontMetrics>
#include <QMenu>
#include <QUrl>
#include <QWidget>

RecentMediaMenu::RecentMediaMenu(RecentMediaHistory &history, QWidget *window)
    : QObject(window), history_(history), menu_(new QMenu(tr("最近播放"), window)),
      reopen_(new QAction(tr("重新打开最近媒体"), this)) {
    menu_->setObjectName(QStringLiteral("recentMediaMenu"));
    menu_->setToolTipsVisible(true);
    reopen_->setObjectName(QStringLiteral("reopenRecentAction"));
    reopen_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    reopen_->setAutoRepeat(false);
    reopen_->setEnabled(!history_.entries().isEmpty());
    window->addAction(reopen_);
    connect(reopen_, &QAction::triggered, this, [this] {
        if (!history_.entries().isEmpty())
            emit openRequested(history_.entries().constFirst());
    });
    connect(&history_, &RecentMediaHistory::changed, this,
            [this] { reopen_->setEnabled(!history_.entries().isEmpty()); });
    // Rebuild only on opening: never delete an action while it is being triggered.
    connect(menu_, &QMenu::aboutToShow, this, &RecentMediaMenu::rebuild);
}

QMenu *RecentMediaMenu::menu() const { return menu_; }

void RecentMediaMenu::rebuild() {
    menu_->clear();
    menu_->addAction(reopen_);
    menu_->addSeparator();
    int index = 0;
    for (const auto &source : history_.entries()) {
        auto name = MediaSource::displayName(source);
        if (name.isEmpty())
            name = tr("未命名媒体");
        for (auto &character : name) {
            if (!character.isPrint())
                character = QLatin1Char(' ');
        }
        name = QFontMetrics(menu_->font()).elidedText(name, Qt::ElideMiddle, 420);
        name.replace(QLatin1String("&"), QLatin1String("&&"));
        auto *action = menu_->addAction(QStringLiteral("%1. %2").arg(++index).arg(name));
        action->setObjectName(QStringLiteral("recentMediaAction%1").arg(index));
        const auto location =
            MediaSource::isNetwork(source)
                ? QUrl(source).toDisplayString(QUrl::RemoveUserInfo | QUrl::RemoveQuery |
                                               QUrl::RemoveFragment)
                : source;
        action->setToolTip(location.toHtmlEscaped());
        connect(action, &QAction::triggered, this, [this, source] { emit openRequested(source); });
    }
    if (index == 0) {
        auto *empty = menu_->addAction(tr("暂无播放记录"));
        empty->setEnabled(false);
    }
    menu_->addSeparator();
    auto *clear = menu_->addAction(tr("清空最近播放记录"));
    clear->setObjectName(QStringLiteral("clearRecentMediaAction"));
    clear->setEnabled(index > 0);
    connect(clear, &QAction::triggered, this, [this] {
        history_.clear();
        emit feedback(tr("最近播放记录已清空"));
    });
}
