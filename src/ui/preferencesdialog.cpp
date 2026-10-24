#include "preferencesdialog.h"
#include "playback/mpvcontroller.h"

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
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(MpvController &player, QWidget *parent,
                                     MediaLibraryPreferences initialMediaLibrary,
                                     bool resumePlaybackPosition, int seekStepSeconds,
                                     AppearancePreferences::Settings appearance)
    : QDialog(parent), appearanceBaseline_(appearance), appearancePalette_(qApp->palette()),
      appearanceStyleSheet_(qApp->styleSheet()), player_(player), tabs_(new QTabWidget(this)),
      hardwareDecoding_(new QComboBox(this)), actualDecoder_(new QLabel(this)),
      audioDevice_(new QComboBox(this)), subtitleVisible_(new QCheckBox(tr("显示字幕"), this)),
      subtitleScale_(new QDoubleSpinBox(this)), subtitlePosition_(new QDoubleSpinBox(this)),
      subtitleFont_(new QFontComboBox(this)),
      mediaScanningEnabled_(new QCheckBox(tr("允许扫描文件夹中的媒体文件"), this)),
      recursiveFolderScan_(new QCheckBox(tr("默认包含子文件夹"), this)),
      resumePlaybackPosition_(new QCheckBox(tr("恢复上次播放位置"), this)),
      seekStepSeconds_(new QComboBox(this)), mediaLibraryPreferences_(initialMediaLibrary),
      resumePlaybackPositionDefault_(resumePlaybackPosition),
      seekStepSecondsDefault_(seekStepSeconds) {
    setObjectName(QStringLiteral("preferencesDialog"));
    setWindowTitle(tr("偏好设置"));
    tabs_->setObjectName(QStringLiteral("preferencesCategories"));
    auto *playback = new QWidget(tabs_);
    auto *playbackForm = new QFormLayout(playback);
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
    for (const int seconds : {5, 10, 30})
        seekStepSeconds_->addItem(tr("%1 秒").arg(seconds), seconds);
    const auto seekIndex = seekStepSeconds_->findData(seekStepSeconds);
    seekStepSeconds_->setCurrentIndex(seekIndex >= 0 ? seekIndex : 0);
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
    audioDevice_->setObjectName(QStringLiteral("audioOutputDeviceChoice"));
    audioDevice_->setAccessibleName(tr("音频输出设备"));
    audioForm->addRow(tr("输出设备："), audioDevice_);
    auto *audioHelp =
        new QLabel(tr("输出设备切换会立即生效；设备不可用时将回退到自动选择。"), audio);
    audioHelp->setWordWrap(true);
    audioForm->addRow(audioHelp);
    tabs_->addTab(audio, style()->standardIcon(QStyle::SP_MediaVolume), tr("音频"));

    auto *subtitles = new QWidget(tabs_);
    auto *subtitleForm = new QFormLayout(subtitles);
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
    auto *subtitleHelp =
        new QLabel(tr("隐藏字幕会保留轨道选择。位置越小越靠上，100% "
                      "为原始位置。默认字体仅用于未指定字体的文本字幕；ASS 保留自带样式。"),
                   subtitles);
    subtitleHelp->setWordWrap(true);
    subtitleForm->addRow(subtitleHelp);
    tabs_->addTab(subtitles, style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("字幕"));

    auto *mediaLibraryPage = new QWidget(tabs_);
    auto *mediaLibraryForm = new QFormLayout(mediaLibraryPage);
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

    auto *layout = new QVBoxLayout(this);
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
        restoreDefaultsButton_->setVisible(index == static_cast<int>(Category::Subtitles) ||
                                           index == static_cast<int>(Category::Appearance));
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
    connect(seekStepSeconds_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(subtitleVisible_, &QCheckBox::toggled, this, &PreferencesDialog::updateApplyButton);
    connect(subtitleScale_, &QDoubleSpinBox::valueChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(subtitlePosition_, &QDoubleSpinBox::valueChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(subtitleFont_, &QFontComboBox::currentTextChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(mediaScanningEnabled_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateApplyButton);
    connect(recursiveFolderScan_, &QCheckBox::toggled, this, &PreferencesDialog::updateApplyButton);
    connect(&player_, &MpvController::hardwareDecodingPreferenceChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(&player_, &MpvController::subtitlePreferencesChanged, this,
            &PreferencesDialog::updateApplyButton);
    connect(&player_, &MpvController::stateChanged, this, [this] {
        updateApplyButton();
        updateActualDecoder();
    });
    connect(applyButton_, &QPushButton::clicked, this, &PreferencesDialog::apply);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::rejected, this, [this] {
        qApp->setPalette(appearancePalette_);
        qApp->setStyleSheet(appearanceStyleSheet_);
    });
    connect(restoreDefaultsButton_, &QPushButton::clicked, this, [this] {
        if (tabs_->currentIndex() == static_cast<int>(Category::Appearance)) {
            appearanceTheme_->setCurrentIndex(0);
            appearanceAccent_->setCurrentIndex(0);
            appearanceGlass_->setChecked(true);
            return;
        }
        subtitleVisible_->setChecked(true);
        subtitleScale_->setValue(1.0);
        subtitlePosition_->setValue(100.0);
        subtitleFont_->setEditText(QString());
        seekStepSeconds_->setCurrentIndex(seekStepSeconds_->findData(5));
    });
    updateApplyButton();
    updateActualDecoder();
    resize(560, sizeHint().height());
}

void PreferencesDialog::selectCategory(Category category) {
    tabs_->setCurrentIndex(static_cast<int>(category));
}

void PreferencesDialog::apply() {
    if (appearanceChanged()) {
        appearanceBaseline_ = appearanceDraft();
        AppearancePreferences::applySettings(*qApp, appearanceBaseline_);
        appearancePalette_ = qApp->palette();
        appearanceStyleSheet_ = qApp->styleSheet();
        emit appearancePreferencesChanged(appearanceBaseline_);
    }
    const bool decodingChanged =
        hardwareDecoding_->currentData().toBool() != player_.hardwareDecodingEnabled();
    if (decodingChanged)
        player_.setHardwareDecodingEnabled(hardwareDecoding_->currentData().toBool());
    if (resumePlaybackPosition_->isChecked() != resumePlaybackPositionDefault_) {
        resumePlaybackPositionDefault_ = resumePlaybackPosition_->isChecked();
        emit resumePlaybackPositionChanged(resumePlaybackPositionDefault_);
    }
    const auto seekStep = seekStepSeconds_->currentData().toInt();
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
        seekStepSeconds_->currentData().toInt() != seekStepSecondsDefault_;
    applyButton_->setEnabled(appearanceChanged() || mediaLibraryChanged ||
                             (player_.isReady() && playbackChange));
}

void PreferencesDialog::createAppearancePage() {
    auto *page = new QWidget(tabs_);
    auto *form = new QFormLayout(page);
    appearanceTheme_ = new QComboBox(page);
    appearanceAccent_ = new QComboBox(page);
    appearanceGlass_ = new QCheckBox(tr("启用玻璃质感"), page);
    appearanceTheme_->setObjectName(QStringLiteral("appearanceThemeChoice"));
    appearanceTheme_->setAccessibleName(tr("基础主题"));
    appearanceAccent_->setObjectName(QStringLiteral("appearanceAccentChoice"));
    appearanceAccent_->setAccessibleName(tr("强调色"));
    appearanceGlass_->setObjectName(QStringLiteral("appearanceGlassChoice"));
    appearanceGlass_->setAccessibleName(tr("启用玻璃质感"));
    for (const auto theme : AppearancePreferences::themes())
        appearanceTheme_->addItem(AppearancePreferences::themeName(theme), static_cast<int>(theme));
    for (const auto accent : AppearancePreferences::accents())
        appearanceAccent_->addItem(AppearancePreferences::accentName(accent),
                                   static_cast<int>(accent));
    appearanceTheme_->setCurrentIndex(
        appearanceTheme_->findData(static_cast<int>(appearanceBaseline_.theme)));
    appearanceAccent_->setCurrentIndex(
        appearanceAccent_->findData(static_cast<int>(appearanceBaseline_.accent)));
    appearanceGlass_->setChecked(appearanceBaseline_.glassEnabled);
    form->addRow(tr("基础主题："), appearanceTheme_);
    form->addRow(tr("强调色："), appearanceAccent_);
    form->addRow(appearanceGlass_);
    auto *help = new QLabel(tr("更改会立即预览，应用后保存。玻璃质感仅作用于界面面板，不会模糊视频"
                               "；不支持背景模糊时使用半透明样式。"),
                            page);
    help->setWordWrap(true);
    form->addRow(help);
    tabs_->addTab(page, style()->standardIcon(QStyle::SP_DesktopIcon), tr("外观"));
    connect(appearanceTheme_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::previewAppearance);
    connect(appearanceAccent_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::previewAppearance);
    connect(appearanceGlass_, &QCheckBox::toggled, this, &PreferencesDialog::previewAppearance);
}

AppearancePreferences::Settings PreferencesDialog::appearanceDraft() const {
    return {static_cast<AppearancePreferences::Theme>(appearanceTheme_->currentData().toInt()),
            static_cast<AppearancePreferences::Accent>(appearanceAccent_->currentData().toInt()),
            appearanceGlass_->isChecked()};
}

bool PreferencesDialog::appearanceChanged() const {
    const auto draft = appearanceDraft();
    return draft.theme != appearanceBaseline_.theme || draft.accent != appearanceBaseline_.accent ||
           draft.glassEnabled != appearanceBaseline_.glassEnabled;
}

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
