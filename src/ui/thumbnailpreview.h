#pragma once

#include <QImage>
#include <QPropertyAnimation>
#include <QWidget>

// A lightweight, non-activating preview surface. QPixmap creation stays in the GUI paint path.
class ThumbnailPreview final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal contentOpacity READ contentOpacity WRITE setContentOpacity)
  public:
    explicit ThumbnailPreview(QWidget *parent = nullptr);

    void present(const QImage &image, double seconds);
    void clear();
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] qreal contentOpacity() const;

  protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    void animateContentTo(qreal opacity);
    void setContentOpacity(qreal opacity);

    QImage image_;
    double seconds_ = 0.0;
    qreal contentOpacity_ = 0.0;
    QPropertyAnimation *animation_;
};
