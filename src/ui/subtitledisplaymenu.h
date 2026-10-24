#pragma once

#include "preferencesdialog.h"
#include <QObject>
#include <functional>

class MpvController;
class QAction;
class QMenu;
class QWidget;

class SubtitleDisplayMenu final : public QObject {
    Q_OBJECT
  public:
    explicit SubtitleDisplayMenu(
        MpvController &controller, QWidget *window,
        std::function<void(PreferencesDialog::Category)> showPreferences = {});
    [[nodiscard]] QMenu *menu() const;
    [[nodiscard]] QAction *settingsAction() const;
    void retranslate();

  signals:
    void feedback(const QString &message);

  private:
    void update();
    MpvController &controller_;
    QMenu *menu_;
    QAction *visible_;
    QAction *settings_;
    std::function<void(PreferencesDialog::Category)> showPreferences_;
};
