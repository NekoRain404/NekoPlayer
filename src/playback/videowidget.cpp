#include "videowidget.h"
#include "mpvcontroller.h"

#include <QApplication>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QWheelEvent>
#include <mpv/render_gl.h>

namespace {
void *resolveGl(void *context, const char *name) {
    return reinterpret_cast<void *>(static_cast<QOpenGLContext *>(context)->getProcAddress(name));
}
} // namespace

VideoWidget::VideoWidget(MpvController &controller, QWidget *parent)
    : QOpenGLWidget(parent), controller_(controller) {
    setObjectName(QStringLiteral("videoSurface"));
    setMinimumSize(320, 180);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    singleClickTimer_.setSingleShot(true);
    singleClickTimer_.setTimerType(Qt::PreciseTimer);
    connect(&singleClickTimer_, &QTimer::timeout, this, &VideoWidget::singleClicked);
}

VideoWidget::~VideoWidget() {
    closing_ = true;
    cleanup();
}

bool VideoWidget::isReady() const { return renderer_ != nullptr; }
bool VideoWidget::hasFailed() const { return failed_; }

QColor VideoWidget::emptySurfaceClearColor(const QPalette &palette, bool glassEnabled) {
    if (!glassEnabled)
        return QColor(Qt::black);

    auto tint = palette.color(QPalette::Base);
    tint.setAlpha(255);
    return tint;
}

void VideoWidget::showEvent(QShowEvent *event) {
    QOpenGLWidget::showEvent(event);
    QTimer::singleShot(500, this, [this] {
        // Qt does not call initializeGL if creation of its own context fails.
        if (!closing_ && !failed_ && !isValid()) {
            failed_ = true;
            emit renderError(tr("OpenGL 初始化失败，请检查图形驱动或显示环境"));
        }
    });
}

void VideoWidget::changeEvent(QEvent *event) {
    QOpenGLWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange)
        update();
}

void VideoWidget::initializeGL() {
    if (!controller_.isReady()) {
        failed_ = true;
        emit renderError(controller_.errorString());
        return;
    }
    mpv_opengl_init_params gl{resolveGl, context()};
    mpv_render_param parameters[]{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
        {MPV_RENDER_PARAM_INVALID, nullptr}};
    const int result = mpv_render_context_create(&renderer_, controller_.handle(), parameters);
    if (result < 0) {
        failed_ = true;
        emit renderError(QString::fromUtf8(mpv_error_string(result)));
        return;
    }
    // A transient failure (for example a slow first context) must not disable
    // media opening for the rest of the session once rendering works again.
    failed_ = false;
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &VideoWidget::cleanup,
            Qt::DirectConnection);
    // this stays alive through callback deregistration and renderer destruction.
    mpv_render_context_set_update_callback(renderer_, &VideoWidget::onUpdate, this);
    emit ready();
}

void VideoWidget::paintGL() {
    auto *gl = context()->functions();
    // Qt enables blending before paintGL; libmpv expects the GL default.
    gl->glDisable(GL_BLEND);
    if (!renderer_ || !controller_.state().active) {
        const bool glass = qApp->property("glassEnabled").toBool();
        // The video surface is always opaque. Glass belongs to the chrome
        // around it; it must not expose the desktop through an empty surface.
        const auto tint = emptySurfaceClearColor(palette(), glass);
        gl->glClearColor(tint.redF(), tint.greenF(), tint.blueF(), tint.alphaF());
        gl->glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    mpv_opengl_fbo target{static_cast<int>(defaultFramebufferObject()),
                          qRound(width() * devicePixelRatioF()),
                          qRound(height() * devicePixelRatioF()), 0};
    int flip = 1;
    int block = 0;
    mpv_render_param parameters[]{{MPV_RENDER_PARAM_OPENGL_FBO, &target},
                                  {MPV_RENDER_PARAM_FLIP_Y, &flip},
                                  {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
                                  {MPV_RENDER_PARAM_INVALID, nullptr}};
    const int result = mpv_render_context_render(renderer_, parameters);
    // mpv leaves FBO 0 bound; Qt still needs its widget FBO for post-paint work.
    gl->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    if (result < 0)
        emit renderError(QString::fromUtf8(mpv_error_string(result)));
}

void VideoWidget::onUpdate(void *userdata) {
    auto *surface = static_cast<VideoWidget *>(userdata);
    if (surface->closing_.load() || surface->updateQueued_.exchange(true))
        return;
    QMetaObject::invokeMethod(surface, &VideoWidget::processUpdate, Qt::QueuedConnection);
}

void VideoWidget::processUpdate() {
    updateQueued_ = false;
    if (closing_ || !renderer_ || !context() || !context()->isValid())
        return;
    makeCurrent();
    const auto flags = mpv_render_context_update(renderer_);
    if ((flags & MPV_RENDER_UPDATE_FRAME) && (!isVisible() || window()->isMinimized())) {
        int skip = 1;
        int block = 0;
        mpv_render_param parameters[]{{MPV_RENDER_PARAM_SKIP_RENDERING, &skip},
                                      {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
                                      {MPV_RENDER_PARAM_INVALID, nullptr}};
        const int result = mpv_render_context_render(renderer_, parameters);
        if (result < 0)
            emit renderError(QString::fromUtf8(mpv_error_string(result)));
    } else if (flags & MPV_RENDER_UPDATE_FRAME) {
        update();
    }
    doneCurrent();
}

void VideoWidget::cleanup() {
    if (!renderer_)
        return;
    if (!closing_)
        emit contextResetting();
    mpv_render_context_set_update_callback(renderer_, nullptr, nullptr);
    makeCurrent();
    mpv_render_context_free(renderer_);
    renderer_ = nullptr;
    doneCurrent();
    disconnect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &VideoWidget::cleanup);
}

void VideoWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        suppressReleaseClick_ = false;
        event->accept();
    } else if (event->button() == Qt::MiddleButton) {
        emit middleClicked();
        event->accept();
    } else {
        QOpenGLWidget::mousePressEvent(event);
    }
}

void VideoWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QOpenGLWidget::mouseReleaseEvent(event);
        return;
    }
    event->accept();
    if (suppressReleaseClick_) {
        suppressReleaseClick_ = false;
        return;
    }
    // Start after release so a held press cannot count as a click; the delay
    // still lets a following double click cancel the single-click action.
    singleClickTimer_.start(QApplication::doubleClickInterval());
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    singleClickTimer_.stop();
    if (event->button() == Qt::LeftButton) {
        suppressReleaseClick_ = true;
        emit doubleClicked();
        event->accept();
    } else {
        QOpenGLWidget::mouseDoubleClickEvent(event);
    }
}

void VideoWidget::wheelEvent(QWheelEvent *event) {
    const int deltaY = event->angleDelta().y();
    if (deltaY != 0)
        emit wheelScrolled(deltaY);
    event->accept();
}
