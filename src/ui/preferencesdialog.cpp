#include "preferencesdialog.h"
#include "appearancetokens.h"
#include "playback/mpvcontroller.h"
#include "preferencesappearancepage.h"
#include "preferencesformlayout.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(MpvController &player, QWidget *parent,
                                     MediaLibraryPreferences initialMediaLibrary,
                                     bool resumePlaybackPosition, int seekStepSeconds,
                                     AppearancePreferences::Settings appearance,
                                     InteractionPreferences interaction,
                                     LanguagePreferences languages)
    : QDialog(parent), appearanceBaseline_(appearance), appearancePalette_(qApp->palette()),
      appearanceStyleSheet_(qApp->styleSheet()), player_(player), tabs_(new QTabWidget(this)),
      hardwareDecoding_(new QComboBox(this)), actualDecoder_(new QLabel(this)),
      audioDevice_(new QComboBox(this)), subtitleVisible_(new QCheckBox(tr("显示字幕"), this)),
      subtitleScale_(new QDoubleSpinBox(this)), subtitlePosition_(new QDoubleSpinBox(this)),
      subtitleFont_(new QFontComboBox(this)),
      mediaScanningEnabled_(new QCheckBox(tr("允许扫描文件夹中的媒体文件"), this)),
      recursiveFolderScan_(new QCheckBox(tr("默认包含子文件夹"), this)),
      resumePlaybackPosition_(new QCheckBox(tr("恢复上次播放位置"), this)),
      seekStepSeconds_(new QSpinBox(this)), mediaLibraryPreferences_(initialMediaLibrary),
      resumePlaybackPositionDefault_(resumePlaybackPosition),
      seekStepSecondsDefault_(normalizedSeekStepSeconds(seekStepSeconds)),
      languagePreferences_(languages.normalized()), lastApplyActive_(player.state().active) {
    // Snapshot the runtime appearance a rejected dialog must return to.
    setProperty("nekoCommittedGlassEnabled", qApp->property("glassEnabled"));
    setProperty("nekoCommittedGlassOpacity", qApp->property("glassOpacity"));
    setProperty("nekoCommittedGlassBlur", qApp->property("glassBlur"));
    const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
    setObjectName(QStringLiteral("preferencesDialog"));
    setWindowTitle(tr("偏好设置"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    tabs_->setObjectName(QStringLiteral("preferencesCategories"));
    auto *playback = new QWidget(tabs_);
    auto *playbackForm = new QFormLayout(playback);
    configurePreferenceForm(playbackForm, tokens);
    hardwareDecoding_->setObjectName(QStringLiteral("hardwareDecodingChoice"));
    hardwareDecoding_->setAccessibleName(tr("视频解码方式"));
    hardwareDecoding_->addItem(tr("自动（安全）"), true);
    hardwareDecoding_->addItem(tr("软件解码"), false);
    hardwareDecoding_->setCurrentIndex(player_.hardwareDecodingEnabled() ? 0 : 1);
    playbackForm->addRow(tr("视频解码方式(&D)："), hardwareDecoding_);
    resumePlaybackPosition_->setObjectName(QStringLiteral("resumePlaybackPositionChoice"));
    resumePlaybackPosition_->setAccessibleName(tr("恢复上次播放位置"));
    resumePlaybackPosition_->setChecked(resumePlaybackPosition);
    playbackForm->addRow(resumePlaybackPosition_);
    seekStepSeconds_->setObjectName(QStringLiteral("seekStepChoice"));
    seekStepSeconds_->setAccessibleName(tr("左右键跳转时长"));
    seekStepSeconds_->setRange(seekStepSecondsMinimum, seekStepSecondsMaximum);
    seekStepSeconds_->setSuffix(tr(" 秒"));
    seekStepSeconds_->setValue(seekStepSecondsDefault_);
    playbackForm->addRow(tr("左右键跳转时长："), seekStepSeconds_);
    actualDecoder_->setObjectName(QStringLiteral("actualDecoder"));
    actualDecoder_->setTextFormat(Qt::PlainText);
    actualDecoder_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    playbackForm->addRow(tr("实际解码器："), actualDecoder_);
    auto *decodeHelp = new QLabel(tr("自动模式优先尝试安全的硬件解码方式，不可用时回退到软件解码。"
                                     "实际使用方式取决于视频格式与显卡驱动。"),
                                  playback);
    decodeHelp->setWordWrap(true);
    playbackForm->addRow(decodeHelp);
    tabs_->addTab(playback, style()->standardIcon(QStyle::SP_MediaPlay), tr("播放"));

    auto *audio = new QWidget(tabs_);
    auto *audioForm = new QFormLayout(audio);
    configurePreferenceForm(audioForm, tokens);
    audioDevice_->setObjectName(QStringLiteral("audioOutputDeviceChoice"));
    audioDevice_->setAccessibleName(tr("音频输出设备"));
    audioForm->addRow(tr("输出设备："), audioDevice_);
    preferredAudioLanguages_ = new QLineEdit(audio);
    preferredAudioLanguages_->setObjectName(QStringLiteral("preferredAudioLanguagesEdit"));
    preferredAudioLanguages_->setAccessibleName(tr("首选音轨语言"));
    preferredAudioLanguages_->setPlaceholderText(tr("例如：jpn,eng"));
    preferredAudioLanguages_->setText(
        languagePreferences_.audioLanguages.join(QStringLiteral(", ")));
    audioForm->addRow(tr("首选音轨语言："), preferredAudioLanguages_);
    auto *audioHelp =
        new QLabel(tr("输出设备切换会立即生效；设备不可用时将回退到自动选择。"), audio);
    audioHelp->setWordWrap(true);
    audioForm->addRow(audioHelp);
    tabs_->addTab(audio, style()->standardIcon(QStyle::SP_MediaVolume), tr("音频"));

    auto *subtitles = new QWidget(tabs_);
    auto *subtitleForm = new QFormLayout(subtitles);
    configurePreferenceForm(subtitleForm, tokens);
    subtitleVisible_->setObjectName(QStringLiteral("subtitleVisibleChoice"));
    subtitleVisible_->setAccessibleName(tr("显示字幕"));
    subtitleScale_->setObjectName(QStringLiteral("subtitleScaleChoice"));
    subtitleScale_->setAccessibleName(tr("字幕缩放"));
    subtitleScale_->setRange(0.25, 4.0);
    subtitleScale_->setSingleStep(0.1);
    subtitleScale_->setDecimals(2);
    subtitleScale_->setSuffix(tr(" 倍"));
    subtitlePosition_->setObjectName(QStringLiteral("subtitlePositionChoice"));
    subtitlePosition_->setAccessibleName(tr("字幕垂直位置"));
    subtitlePosition_->setRange(0, 100);
    subtitlePosition_->setSingleStep(2);
    subtitlePosition_->setDecimals(1);
    subtitlePosition_->setSuffix(QStringLiteral(" %"));
    subtitleFont_->setObjectName(QStringLiteral("subtitleFontChoice"));
    subtitleFont_->setAccessibleName(tr("默认字幕字体"));
    subtitleFont_->setEditable(true);
    subtitleFont_->lineEdit()->setMaxLength(256);
    subtitleForm->addRow(subtitleVisible_);
    subtitleForm->addRow(tr("缩放(&S)："), subtitleScale_);
    subtitleForm->addRow(tr("垂直位置(&P)："), subtitlePosition_);
    subtitleForm->addRow(tr("默认字体(&F)："), subtitleFont_);
    preferredSubtitleLanguages_ = new QLineEdit(subtitles);
    preferredSubtitleLanguages_->setObjectName(QStringLiteral("preferredSubtitleLanguagesEdit"));
    preferredSubtitleLanguages_->setAccessibleName(tr("首选字幕语言"));
    preferredSubtitleLanguages_->setText(
        languagePreferences_.subtitleLanguages.join(QStringLiteral(", ")));
    subtitleForm->addRow(tr("首选字幕语言："), preferredSubtitleLanguages_);
    autoLoadExternalSubtitles_ = new QCheckBox(tr("自动加载同名字幕"), subtitles);
    autoLoadExternalSubtitles_->setObjectName(QStringLiteral("autoLoadExternalSubtitlesCheck"));
    autoLoadExternalSubtitles_->setAccessibleName(tr("自动加载同名字幕"));
    autoLoadExternalSubtitles_->setChecked(languagePreferences_.autoLoadExternalSubtitles);
    subtitleForm->addRow(autoLoadExternalSubtitles_);
    auto *subtitleHelp =
        new QLabel(tr("隐藏字幕会保留轨道选择。位置越小越靠上，100% "
                      "为原始位置。默认字体仅用于未指定字体的文本字幕；ASS 保留自带样式。"),
                   subtitles);
    subtitleHelp->setWordWrap(true);
    subtitleForm->addRow(subtitleHelp);
    tabs_->addTab(subtitles, style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("字幕"));

    auto *mediaLibraryPage = new QWidget(tabs_);
    auto *mediaLibraryForm = new QFormLayout(mediaLibraryPage);
    configurePreferenceForm(mediaLibraryForm, tokens);
    mediaScanningEnabled_->setObjectName(QStringLiteral("mediaScanningEnabledChoice"));
    mediaScanningEnabled_->setAccessibleName(tr("允许扫描文件夹中的媒体文件"));
    mediaScanningEnabled_->setChecked(initialMediaLibrary.scanningEnabled);
    recursiveFolderScan_->setObjectName(QStringLiteral("recursiveFolderScanChoice"));
    recursiveFolderScan_->setAccessibleName(tr("默认包含子文件夹"));
    recursiveFolderScan_->setChecked(initialMediaLibrary.recursiveByDefault);
    mediaLibraryForm->addRow(mediaScanningEnabled_);
    mediaLibraryForm->addRow(recursiveFolderScan_);
    auto *scanHelp = new QLabel(tr("扫描仅在你打开、添加或拖入文件夹时进行，不会在启动时自动遍历目"
                                   "录。关闭后，文件夹扫描入口将停用；直接打开媒体文件仍可使用。"),
                                mediaLibraryPage);
    scanHelp->setWordWrap(true);
    mediaLibraryForm->addRow(scanHelp);
    tabs_->addTab(mediaLibraryPage, style()->standardIcon(QStyle::SP_DirOpenIcon), tr("媒体库"));
    createAppearancePage();
    createInteractionPage(interaction);
    // Let the shared glass pane show through every settings page instead of an
    // opaque page backing store.
    tabs_->setAttribute(Qt::WA_TranslucentBackground, true);
    for (int index = 0; index < tabs_->count(); ++index) {
        auto *page = tabs_->widget(index);
        if (!page)
            continue;
        page->setAttribute(Qt::WA_TranslucentBackground, true);
        page->setAutoFillBackground(false);
    }
    for (auto *stack : tabs_->findChildren<QStackedWidget *>()) {
        stack->setAttribute(Qt::WA_TranslucentBackground, true);
        stack->setAutoFillBackground(false);
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(tokens.spaceUnit * 4, tokens.spaceUnit * 4, tokens.spaceUnit * 4,
                               tokens.spaceUnit * 4);
    layout->setSpacing(tokens.spaceUnit * 3);
    layout->addWidget(tabs_);
    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                 QDialogButtonBox::Apply | QDialogButtonBox::RestoreDefaults,
                             this);
    buttons->setObjectName(QStringLiteral("preferencesButtons"));
    applyButton_ = buttons->button(QDialogButtonBox::Apply);
    restoreDefaultsButton_ = buttons->button(QDialogButtonBox::RestoreDefaults);
    restoreDefaultsButton_->setVisible(false);
    layout->addWidget(buttons);
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        restoreDefaultsButton_->setVisible(index == static_cast<int>(Category::Audio) ||
                                           index == static_cast<int>(Category::Subtitles) ||
                                           index == static_cast<int>(Category::Appearance) ||
                                           index == static_cast<int>(Category::Interaction));
    });

    refreshAudioDevices();
    displaySubtitlePreferences();
    connect(audioDevice_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0)
            player_.setAudioDevice(audioDevice_->itemData(index).toString());
    });
    connect(&player_, &MpvController::audioDevicesChanged, this,
            &PreferencesDialog::refreshAudioDevices);
    connect(hardwareDecoding_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(resumePlaybackPosition_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateApplyButton);
    connect(seekStepSeconds_, &QSpinBox::valueChanged, this, &PreferencesDialog::updateApplyButton);
    connect(subtitleVisible_, &QCheckBox::toggled, this, &PreferencesDialog::updateApplyButton);
    connect(subtitleScale_, &QDoubleSpinBox::valueChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(subtitlePosition_, &QDoubleSpinBox::valueChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(subtitleFont_, &QFontComboBox::currentTextChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(preferredAudioLanguages_, &QLineEdit::textChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(preferredSubtitleLanguages_, &QLineEdit::textChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(autoLoadExternalSubtitles_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateApplyButton);
    connect(mediaScanningEnabled_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateApplyButton);
    connect(recursiveFolderScan_, &QCheckBox::toggled, this, &PreferencesDialog::updateApplyButton);
    connect(interactionSingleClick_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateApplyButton);
    connect(interactionDoubleClick_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateApplyButton);
    connect(interactionMiddleClick_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(interactionWheelVolume_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateApplyButton);
    connect(interactionWheelStep_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(interactionWheelSeek_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateApplyButton);
    connect(interactionPlaylistClick_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateApplyButton);
    connect(interactionRemainingTime_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateApplyButton);
    connect(&player_, &MpvController::hardwareDecodingPreferenceChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(&player_, &MpvController::subtitlePreferencesChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(&player_, &MpvController::stateChanged, this, [this] {
        // Position ticks fire continuously; only an active-state transition
        // can change the Apply draft. Decoder text keeps its own early-out.
        const bool active = player_.state().active;
        if (active != lastApplyActive_) {
            lastApplyActive_ = active;
            updateApplyButton();
        }
        updateActualDecoder();
    });
    connect(applyButton_, &QPushButton::clicked, this, &PreferencesDialog::apply);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::rejected, this, [this] {
        if (auto *timer = findChild<QTimer *>(QStringLiteral("appearancePreviewTimer")))
            timer->stop();
        qApp->setPalette(appearancePalette_);
        qApp->setStyleSheet(appearanceStyleSheet_);
        qApp->setProperty("glassEnabled", property("nekoCommittedGlassEnabled"));
        qApp->setProperty("glassOpacity", property("nekoCommittedGlassOpacity"));
        qApp->setProperty("glassBlur", property("nekoCommittedGlassBlur"));
    });
    connect(restoreDefaultsButton_, &QPushButton::clicked, this, [this] {
        // Reset only the category whose page is showing; hidden pages keep their drafts.
        switch (static_cast<Category>(tabs_->currentIndex())) {
        case Category::Audio:
            preferredAudioLanguages_->clear();
            break;
        case Category::Subtitles:
            subtitleVisible_->setChecked(true);
            subtitleScale_->setValue(1.0);
            subtitlePosition_->setValue(100.0);
            subtitleFont_->setEditText(SubtitlePreferences{}.font);
            preferredSubtitleLanguages_->clear();
            autoLoadExternalSubtitles_->setChecked(false);
            break;
        case Category::Appearance:
            appearancePage_->restoreDefaults();
            break;
        case Category::Interaction:
            displayInteractionPreferences(InteractionPreferences{});
            break;
        case Category::Playback:
        case Category::MediaLibrary:
            break;
        }
    });
    updateApplyButton();
    updateActualDecoder();
    alignPreferenceFormLabels(this, tokens);
    resize(560, sizeHint().height());
}

void PreferencesDialog::selectCategory(Category category) {
    tabs_->setCurrentIndex(static_cast<int>(category));
}

void PreferencesDialog::apply() {
    // Flush a throttled preview so Apply and the rollback snapshot see the draft.
    auto *previewTimer = findChild<QTimer *>(QStringLiteral("appearancePreviewTimer"));
    if (previewTimer && previewTimer->isActive()) {
        previewTimer->stop();
        previewAppearance();
    }
    if (appearanceChanged()) {
        appearanceBaseline_ = appearanceDraft();
        appearancePage_->accept();
        emit appearancePreferencesChanged(appearanceBaseline_);
    }
    // Apply commits the live preview; a later Cancel returns to this state.
    appearancePalette_ = qApp->palette();
    appearanceStyleSheet_ = qApp->styleSheet();
    setProperty("nekoCommittedGlassEnabled", qApp->property("glassEnabled"));
    setProperty("nekoCommittedGlassOpacity", qApp->property("glassOpacity"));
    setProperty("nekoCommittedGlassBlur", qApp->property("glassBlur"));
    const bool decodingChanged =
        hardwareDecoding_->currentData().toBool() != player_.hardwareDecodingEnabled();
    if (decodingChanged)
        player_.setHardwareDecodingEnabled(hardwareDecoding_->currentData().toBool());
    if (resumePlaybackPosition_->isChecked() != resumePlaybackPositionDefault_) {
        resumePlaybackPositionDefault_ = resumePlaybackPosition_->isChecked();
        emit resumePlaybackPositionChanged(resumePlaybackPositionDefault_);
    }
    const auto seekStep = seekStepSeconds_->value();
    if (seekStep != seekStepSecondsDefault_) {
        seekStepSecondsDefault_ = seekStep;
        emit seekStepSecondsChanged(seekStep);
    }
    subtitleScale_->interpretText();
    subtitlePosition_->interpretText();
    const SubtitlePreferences draft{subtitleVisible_->isChecked(), subtitleScale_->value(),
                                    subtitlePosition_->value(), subtitleFont_->currentText()};
    if (draft.normalized() != player_.subtitlePreferences())
        player_.setSubtitlePreferences(draft);
    const MediaLibraryPreferences mediaLibraryDraft{mediaScanningEnabled_->isChecked(),
                                                    recursiveFolderScan_->isChecked()};
    if (mediaLibraryDraft.scanningEnabled != mediaLibraryPreferences_.scanningEnabled ||
        mediaLibraryDraft.recursiveByDefault != mediaLibraryPreferences_.recursiveByDefault) {
        mediaLibraryPreferences_ = mediaLibraryDraft;
        emit mediaLibraryPreferencesChanged(mediaLibraryDraft.scanningEnabled,
                                            mediaLibraryDraft.recursiveByDefault);
    }
    if (interactionChanged()) {
        interactionPreferences_ = interactionDraft();
        emit interactionPreferencesChanged(interactionPreferences_);
    }
    if (languageChanged()) {
        languagePreferences_ = languageDraft();
        player_.setLanguagePreferences(languagePreferences_.audioLanguages,
                                       languagePreferences_.subtitleLanguages);
        player_.setAutoLoadExternalSubtitles(languagePreferences_.autoLoadExternalSubtitles);
        emit languagePreferencesChanged(languagePreferences_);
    }
    displaySubtitlePreferences();
    updateApplyButton();
}

void PreferencesDialog::updateApplyButton() {
    const bool decodingChanged =
        hardwareDecoding_->currentData().toBool() != player_.hardwareDecodingEnabled();
    const SubtitlePreferences draft{subtitleVisible_->isChecked(), subtitleScale_->value(),
                                    subtitlePosition_->value(), subtitleFont_->currentText()};
    const bool mediaLibraryChanged =
        mediaScanningEnabled_->isChecked() != mediaLibraryPreferences_.scanningEnabled ||
        recursiveFolderScan_->isChecked() != mediaLibraryPreferences_.recursiveByDefault;
    const bool playbackChange =
        decodingChanged || draft.normalized() != player_.subtitlePreferences() ||
        resumePlaybackPosition_->isChecked() != resumePlaybackPositionDefault_ ||
        seekStepSeconds_->value() != seekStepSecondsDefault_;
    applyButton_->setEnabled(appearanceChanged() || mediaLibraryChanged || interactionChanged() ||
                             languageChanged() || (player_.isReady() && playbackChange));
}

void PreferencesDialog::createAppearancePage() {
    appearancePage_ = new PreferencesAppearancePage(appearanceBaseline_, tabs_);
    auto *previewTimer = new QTimer(this);
    previewTimer->setObjectName(QStringLiteral("appearancePreviewTimer"));
    previewTimer->setSingleShot(true);
    previewTimer->setInterval(120);
    connect(previewTimer, &QTimer::timeout, this, &PreferencesDialog::previewAppearance);
    connect(appearancePage_, &PreferencesAppearancePage::edited, this, [this, previewTimer] {
        // Leading-edge throttle: the first change of a gesture previews
        // immediately (so combo/checkbox edits feel instant), while the rest of
        // a slider drag is coalesced into one restyle per interval.
        updateApplyButton();
        if (previewTimer->isActive())
            return;
        previewAppearance();
        previewTimer->start();
    });
    tabs_->addTab(appearancePage_, style()->standardIcon(QStyle::SP_DesktopIcon), tr("外观"));
}

AppearancePreferences::Settings PreferencesDialog::appearanceDraft() const {
    return appearancePage_->draft();
}

bool PreferencesDialog::appearanceChanged() const { return appearancePage_->changed(); }

void PreferencesDialog::previewAppearance() {
    AppearancePreferences::applySettings(*qApp, appearanceDraft());
    updateApplyButton();
}

void PreferencesDialog::updateActualDecoder() {
    const auto &state = player_.state();
    if (lastActive_ == state.active && lastDecoder_ == state.hardwareDecoder &&
        !actualDecoder_->text().isEmpty())
        return;
    lastActive_ = state.active;
    lastDecoder_ = state.hardwareDecoder;
    if (!state.active)
        actualDecoder_->setText(tr("未播放"));
    else if (state.hardwareDecoder.isEmpty())
        actualDecoder_->setText(tr("未检测到视频解码器"));
    else if (state.hardwareDecoder == QLatin1String("no"))
        actualDecoder_->setText(tr("软件解码"));
    else
        actualDecoder_->setText(state.hardwareDecoder);
}

void PreferencesDialog::refreshAudioDevices() {
    const auto selected = player_.selectedAudioDevice();
    const QSignalBlocker blocker(audioDevice_);
    audioDevice_->clear();
    for (const auto &device : player_.audioDevices()) {
        const auto label = device.description.isEmpty() ? device.name : device.description;
        audioDevice_->addItem(label, device.name);
    }
    const int index = audioDevice_->findData(selected);
    if (index >= 0)
        audioDevice_->setCurrentIndex(index);
    audioDevice_->setEnabled(audioDevice_->count() > 0);
}

void PreferencesDialog::displaySubtitlePreferences() {
    const auto &preferences = player_.subtitlePreferences();
    subtitleVisible_->setChecked(preferences.visible);
    subtitleScale_->setValue(preferences.scale);
    subtitlePosition_->setValue(preferences.position);
    subtitleFont_->setEditText(preferences.font);
}

void PreferencesDialog::createInteractionPage(InteractionPreferences interaction) {
    auto *page = new QWidget(tabs_);
    auto *form = new QFormLayout(page);
    configurePreferenceForm(form, appearanceTokens(AppearancePreferences::Theme::Dark));
    interactionSingleClick_ = new QCheckBox(tr("单击视频时播放或暂停"), page);
    interactionSingleClick_->setObjectName(QStringLiteral("interactionSingleClickChoice"));
    interactionSingleClick_->setAccessibleName(tr("单击视频时播放或暂停"));
    interactionDoubleClick_ = new QCheckBox(tr("双击视频时切换全屏"), page);
    interactionDoubleClick_->setObjectName(QStringLiteral("interactionDoubleClickChoice"));
    interactionDoubleClick_->setAccessibleName(tr("双击视频时切换全屏"));
    interactionMiddleClick_ = new QComboBox(page);
    interactionMiddleClick_->setObjectName(QStringLiteral("interactionMiddleClickChoice"));
    interactionMiddleClick_->setAccessibleName(tr("中键单击操作"));
    interactionMiddleClick_->addItem(tr("无操作"), static_cast<int>(MiddleClickAction::None));
    interactionMiddleClick_->addItem(tr("播放/暂停"),
                                     static_cast<int>(MiddleClickAction::PlayPause));
    interactionMiddleClick_->addItem(tr("静音"), static_cast<int>(MiddleClickAction::Mute));
    interactionMiddleClick_->addItem(tr("全屏"), static_cast<int>(MiddleClickAction::Fullscreen));
    interactionWheelVolume_ = new QCheckBox(tr("滚轮调节音量"), page);
    interactionWheelVolume_->setObjectName(QStringLiteral("interactionWheelVolumeChoice"));
    interactionWheelVolume_->setAccessibleName(tr("滚轮调节音量"));
    interactionWheelStep_ = new QComboBox(page);
    interactionWheelStep_->setObjectName(QStringLiteral("interactionWheelStepChoice"));
    interactionWheelStep_->setAccessibleName(tr("音量步进"));
    for (int step = 1; step <= 20; ++step)
        interactionWheelStep_->addItem(tr("%1%").arg(step), step);
    interactionWheelSeek_ = new QCheckBox(tr("时间轴滚轮跳转"), page);
    interactionWheelSeek_->setObjectName(QStringLiteral("interactionWheelSeekChoice"));
    interactionWheelSeek_->setAccessibleName(tr("时间轴滚轮跳转"));
    interactionPlaylistClick_ = new QCheckBox(tr("播放列表单击播放"), page);
    interactionPlaylistClick_->setObjectName(QStringLiteral("interactionPlaylistClickChoice"));
    interactionPlaylistClick_->setAccessibleName(tr("播放列表单击播放"));
    interactionRemainingTime_ = new QCheckBox(tr("显示剩余时间"), page);
    interactionRemainingTime_->setObjectName(QStringLiteral("interactionRemainingTimeChoice"));
    interactionRemainingTime_->setAccessibleName(tr("显示剩余时间"));
    form->addRow(interactionSingleClick_);
    form->addRow(interactionDoubleClick_);
    form->addRow(tr("中键单击："), interactionMiddleClick_);
    form->addRow(interactionWheelVolume_);
    form->addRow(tr("音量步进："), interactionWheelStep_);
    form->addRow(interactionWheelSeek_);
    form->addRow(interactionPlaylistClick_);
    form->addRow(interactionRemainingTime_);
    auto *help =
        new QLabel(tr("单击会等待双击间隔，避免双击时也触发播放/暂停；关闭后该操作不执行。"), page);
    help->setWordWrap(true);
    form->addRow(help);
    tabs_->addTab(page, style()->standardIcon(QStyle::SP_FileDialogListView), tr("交互"));
    interactionPreferences_ = interaction.normalized();
    displayInteractionPreferences(interactionPreferences_);
}

void PreferencesDialog::displayInteractionPreferences(const InteractionPreferences &preferences) {
    interactionSingleClick_->setChecked(preferences.singleClickPlayPause);
    interactionDoubleClick_->setChecked(preferences.doubleClickFullscreen);
    const auto middleIndex =
        interactionMiddleClick_->findData(static_cast<int>(preferences.middleClick));
    interactionMiddleClick_->setCurrentIndex(middleIndex >= 0 ? middleIndex : 0);
    interactionWheelVolume_->setChecked(preferences.wheelAdjustsVolume);
    const auto stepIndex = interactionWheelStep_->findData(preferences.wheelVolumeStep);
    interactionWheelStep_->setCurrentIndex(
        stepIndex >= 0 ? stepIndex
                       : interactionWheelStep_->findData(InteractionPreferences{}.wheelVolumeStep));
    interactionWheelSeek_->setChecked(preferences.wheelSeeksTimeline);
    interactionPlaylistClick_->setChecked(preferences.playlistSingleClick);
    interactionRemainingTime_->setChecked(preferences.showRemainingTime);
}

InteractionPreferences PreferencesDialog::interactionDraft() const {
    return {.singleClickPlayPause = interactionSingleClick_->isChecked(),
            .doubleClickFullscreen = interactionDoubleClick_->isChecked(),
            .middleClick =
                static_cast<MiddleClickAction>(interactionMiddleClick_->currentData().toInt()),
            .wheelAdjustsVolume = interactionWheelVolume_->isChecked(),
            .wheelVolumeStep = interactionWheelStep_->currentData().toInt(),
            .wheelSeeksTimeline = interactionWheelSeek_->isChecked(),
            .playlistSingleClick = interactionPlaylistClick_->isChecked(),
            .showRemainingTime = interactionRemainingTime_->isChecked()};
}

bool PreferencesDialog::interactionChanged() const {
    return interactionDraft() != interactionPreferences_;
}

LanguagePreferences PreferencesDialog::languageDraft() const {
    const auto parse = [](const QString &text) {
        return LanguagePreferences{text.split(QLatin1Char(',')), {}, false}
            .normalized()
            .audioLanguages;
    };
    return {parse(preferredAudioLanguages_->text()), parse(preferredSubtitleLanguages_->text()),
            autoLoadExternalSubtitles_->isChecked()};
}

bool PreferencesDialog::languageChanged() const { return languageDraft() != languagePreferences_; }
