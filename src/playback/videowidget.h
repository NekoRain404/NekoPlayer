#pragma once

#include <QOpenGLWidget>
#include <atomic>

class MpvController;
struct mpv_render_context;

// Owns only GPU resources. The controller must outlive this surface.
class VideoWidget final : public QOpenGLWidget {
    Q_OBJECT
  public:
    explicit VideoWidget(MpvController &controller, QWidget *parent = nullptr);
    ~VideoWidget() override;
    [[nodiscard]] bool isReady() const;
    [[nodiscard]] bool hasFailed() const;

  signals:
    void ready();
    void renderError(const QString &message);
    void contextResetting();
    void doubleClicked();

  protected:
    void initializeGL() override;
    void paintGL() override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    static void onUpdate(void *userdata);
    void processUpdate();
    void cleanup();
    MpvController &controller_;
    mpv_render_context *renderer_ = nullptr;
    std::atomic_bool closing_{false};
    bool failed_ = false;
};
