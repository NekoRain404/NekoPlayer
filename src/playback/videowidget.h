#pragma once

#include <QOpenGLWidget>
#include <QTimer>
#include <atomic>

class MpvController;
class QMouseEvent;
class QWheelEvent;
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
    void singleClicked();
    void doubleClicked();
    void middleClicked();
    void wheelScrolled(int deltaY);

  protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    static void onUpdate(void *userdata);
    void processUpdate();
    void cleanup();
    MpvController &controller_;
    mpv_render_context *renderer_ = nullptr;
    std::atomic_bool closing_{false};
    QTimer singleClickTimer_;
    bool failed_ = false;
};
