#include "trackmenus.h"
#include "playback/mpvcontroller.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QMenu>
#include <QWidget>

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

TrackMenus::TrackMenus(MpvController &controller, QWidget *window, QMenu *subtitleDelayMenu,
                       QMenu *subtitleDisplayMenu)
    : QObject(window), controller_(controller), window_(window),
      audioMenu_(new QMenu(tr("音轨"), window)), subtitleMenu_(new QMenu(tr("字幕(&U)"), window)),
      subtitleDelayMenu_(subtitleDelayMenu), subtitleDisplayMenu_(subtitleDisplayMenu),
      audioGroup_(new QActionGroup(audioMenu_)), subtitleGroup_(new QActionGroup(subtitleMenu_)),
      loadAudioAction_(new QAction(tr("加载外挂音轨…"), this)),
      loadSubtitleAction_(new QAction(tr("加载外挂字幕…"), this)) {
    audioMenu_->setObjectName(QStringLiteral("audioTracksMenu"));
    subtitleMenu_->setObjectName(QStringLiteral("subtitleTracksMenu"));
    loadAudioAction_->setObjectName(QStringLiteral("loadAudioAction"));
    loadAudioAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    window->addAction(loadAudioAction_);
    loadSubtitleAction_->setObjectName(QStringLiteral("loadSubtitleAction"));
    loadSubtitleAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    window->addAction(loadSubtitleAction_);
    connect(loadAudioAction_, &QAction::triggered, this,
            [this] { chooseExternalTrack(MediaTrack::Type::Audio); });
    connect(loadSubtitleAction_, &QAction::triggered, this,
            [this] { chooseExternalTrack(MediaTrack::Type::Subtitle); });
    connect(&controller_, &MpvController::fileOpenRequested, this, [this] { ++sourceRevision_; });
    connect(&controller_, &MpvController::stopRequested, this, [this] { ++sourceRevision_; });
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
    audioMenu_->addAction(loadAudioAction_);
    audioMenu_->addSeparator();
    populate(audioMenu_, audioGroup_, MediaTrack::Type::Audio);
    subtitleMenu_->addAction(loadSubtitleAction_);
    subtitleMenu_->addSeparator();
    populate(subtitleMenu_, subtitleGroup_, MediaTrack::Type::Subtitle);
    subtitleMenu_->addSeparator();
    subtitleMenu_->addMenu(subtitleDelayMenu_);
    subtitleMenu_->addMenu(subtitleDisplayMenu_);
    updateAvailability();
}

void TrackMenus::updateAvailability() {
    active_ = controller_.state().active;
    audioMenu_->setEnabled(active_);
    loadAudioAction_->setEnabled(active_);
    subtitleMenu_->setEnabled(active_);
    loadSubtitleAction_->setEnabled(active_);
}

void TrackMenus::chooseExternalTrack(MediaTrack::Type type) {
    if (!controller_.state().active)
        return;
    const auto revision = sourceRevision_;
    const auto source = controller_.state().path;
    const bool audio = type == MediaTrack::Type::Audio;
    const auto path = QFileDialog::getOpenFileName(
        window_, audio ? tr("加载外挂音轨") : tr("加载外挂字幕"), {},
        audio ? tr("音频文件 (*.mka *.aac *.ac3 *.eac3 *.dts *.dtshd *.flac *.mp3 *.ogg *.opus "
                   "*.wav *.m4a *.mp4 *.mkv);;所有文件 (*)")
              : tr("字幕文件 (*.srt *.ass *.ssa *.sub *.vtt *.sup *.idx *.mks);;所有文件 (*)"));
    if (path.isEmpty() || revision != sourceRevision_ || !controller_.state().active ||
        source != controller_.state().path)
        return;
    if (audio)
        controller_.addAudio(path);
    else
        controller_.addSubtitle(path);
}
