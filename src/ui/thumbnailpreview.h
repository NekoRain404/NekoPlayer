#pragma once

#include <QImage>
#include <QWidget>

// A lightweight, non-activating preview surface. QPixmap creation stays in the GUI paint path.
class ThumbnailPreview final : public QWidget {
    Q_OBJECT
  public:
    explicit ThumbnailPreview(QWidget *parent = nullptr);

    void present(const QImage &image, double seconds);
    void clear();
    [[nodiscard]] QSize sizeHint() const override;

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QImage image_;
    double seconds_ = 0.0;
};
