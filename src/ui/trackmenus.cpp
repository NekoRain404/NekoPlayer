#include "trackmenus.h"
#include "playback/mpvcontroller.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QMenu>
#include <QWidget>
#include <algorithm>

namespace {
QString trackLabel(const MediaTrack &track) {
    QStringList details;
    if (!track.title.isEmpty())
        details.append(track.title);
    if (!track.language.isEmpty())
        details.append(track.language);
    if (!track.codec.isEmpty())
        details.append(track.codec);
    if (track.external)
        details.append(TrackMenus::tr("外挂"));
    auto label = TrackMenus::tr("轨道 %1").arg(track.id);
    if (!details.isEmpty())
        label += QStringLiteral(" · ") + details.join(QStringLiteral(" / "));
    return label.replace(QLatin1Char('&'), QStringLiteral("&&"));
}
} // namespace

TrackMenus::TrackMenus(MpvController &controller, QWidget *window)
    : QObject(window), controller_(controller), window_(window),
      audioMenu_(new QMenu(tr("音轨"), window)), subtitleMenu_(new QMenu(tr("字幕(&S)"), window)),
      audioGroup_(new QActionGroup(audioMenu_)), subtitleGroup_(new QActionGroup(subtitleMenu_)),
      loadSubtitleAction_(new QAction(tr("加载外挂字幕…"), this)) {
    audioMenu_->setObjectName(QStringLiteral("audioTracksMenu"));
    subtitleMenu_->setObjectName(QStringLiteral("subtitleTracksMenu"));
    loadSubtitleAction_->setObjectName(QStringLiteral("loadSubtitleAction"));
    loadSubtitleAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    window->addAction(loadSubtitleAction_);
    connect(loadSubtitleAction_, &QAction::triggered, this, &TrackMenus::chooseSubtitle);
    connect(audioGroup_, &QActionGroup::triggered, this,
            [this](QAction *action) { controller_.selectAudioTrack(action->data().toLongLong()); });
    connect(subtitleGroup_, &QActionGroup::triggered, this, [this](QAction *action) {
        controller_.selectSubtitleTrack(action->data().toLongLong());
    });
    connect(&controller_, &MpvController::tracksChanged, this, &TrackMenus::rebuild);
    connect(&controller_, &MpvController::stateChanged, this, [this] {
        if (active_ != controller_.state().active)
            updateAvailability();
    });
    rebuild();
}

QMenu *TrackMenus::audioMenu() const { return audioMenu_; }
QMenu *TrackMenus::subtitleMenu() const { return subtitleMenu_; }

void TrackMenus::populate(QMenu *menu, QActionGroup *group, MediaTrack::Type type) {
    const auto prefix = type == MediaTrack::Type::Audio ? QStringLiteral("audioTrack")
                                                        : QStringLiteral("subtitleTrack");
    auto *off = menu->addAction(tr("关闭"));
    off->setObjectName(prefix + QStringLiteral("0Action"));
    off->setData(qint64{0});
    off->setCheckable(true);
    group->addAction(off);
    bool selected = false;
    for (const auto &track : controller_.tracks()) {
        if (track.type != type)
            continue;
        auto *action = menu->addAction(trackLabel(track));
        action->setObjectName(prefix + QString::number(track.id) + QStringLiteral("Action"));
        action->setData(track.id);
        action->setCheckable(true);
        group->addAction(action);
        action->setChecked(track.selected);
        selected |= track.selected;
    }
    off->setChecked(!selected);
}

void TrackMenus::rebuild() {
    audioMenu_->clear();
    subtitleMenu_->clear();
    populate(audioMenu_, audioGroup_, MediaTrack::Type::Audio);
    subtitleMenu_->addAction(loadSubtitleAction_);
    subtitleMenu_->addSeparator();
    populate(subtitleMenu_, subtitleGroup_, MediaTrack::Type::Subtitle);
    updateAvailability();
}

void TrackMenus::updateAvailability() {
    active_ = controller_.state().active;
    audioMenu_->setEnabled(
        active_ &&
        std::any_of(controller_.tracks().cbegin(), controller_.tracks().cend(),
                    [](const MediaTrack &track) { return track.type == MediaTrack::Type::Audio; }));
    subtitleMenu_->setEnabled(active_);
    loadSubtitleAction_->setEnabled(active_);
}

void TrackMenus::chooseSubtitle() {
    const auto path = QFileDialog::getOpenFileName(
        window_, tr("加载外挂字幕"), {},
        tr("字幕文件 (*.srt *.ass *.ssa *.sub *.vtt *.sup *.idx *.mks);;所有文件 (*)"));
    if (!path.isEmpty())
        controller_.addSubtitle(path);
}
