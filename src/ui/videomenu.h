#pragma once

#include <QObject>

class MpvController;
enum class DenoiseLevel;
enum class SharpenLevel;
class QAction;
class QActionGroup;
class QMenu;
class QWidget;

class VideoMenu final : public QObject {
    Q_OBJECT
  public:
    explicit VideoMenu(MpvController &controller, QWidget *window);
    [[nodiscard]] QMenu *menu() const;
    void retranslate();

  signals:
    void feedback(const QString &message);

  private:
    void restoreFilters();
    void persistFilters() const;
    void update();
    MpvController &controller_;
    QMenu *menu_;
    QMenu *aspectMenu_;
    QMenu *hdrToneMappingMenu_;
    QMenu *deinterlaceMenu_;
    QMenu *sharpenMenu_;
    QMenu *denoiseMenu_;
    QActionGroup *aspects_;
    QActionGroup *hdrToneMappings_;
    QActionGroup *deinterlaceModes_;
    QActionGroup *sharpenLevels_;
    QActionGroup *denoiseLevels_;
    QAction *resetRotation_;
};
