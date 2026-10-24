#pragma once

#include "preferencesdialog.h"
#include <QObject>
#include <functional>
#include <memory>

class MpvController;
class QWidget;

// Builds and wires one preferences dialog from narrow getters/setters so every
// entry point shows the current values. It owns no storage and calls no mpv
// commands; MainWindow supplies the state accessors.
class PreferencesCoordinator final : public QObject {
    Q_OBJECT
  public:
    struct Handlers {
        std::function<AppearancePreferences::Settings()> appearance;
        std::function<void(AppearancePreferences::Settings)> setAppearance;
        std::function<MediaLibraryPreferences()> mediaLibrary;
        std::function<void(MediaLibraryPreferences)> setMediaLibrary;
        std::function<bool()> resumePlaybackPosition;
        std::function<void(bool)> setResumePlaybackPosition;
        std::function<int()> seekStepSeconds;
        std::function<void(int)> setSeekStepSeconds;
    };

    explicit PreferencesCoordinator(MpvController &player, Handlers handlers,
                                    QObject *parent = nullptr);

    [[nodiscard]] std::unique_ptr<PreferencesDialog>
    createDialog(QWidget *parent, PreferencesDialog::Category category);
    void show(QWidget *parent, PreferencesDialog::Category category);

  private:
    MpvController &player_;
    Handlers handlers_;
};
