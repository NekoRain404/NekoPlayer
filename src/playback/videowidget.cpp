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
    connect(&singleClickTimer_, &QTimer::timeout, this, &VideoWidget::singleClicked);
}

VideoWidget::~VideoWidget() {
    closing_ = true;
    cleanup();
}

bool VideoWidget::isReady() const { return renderer_ != nullptr; }
bool VideoWidget::hasFailed() const { return failed_; }

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
    if (!renderer_) {
        gl->glClearColor(0, 0, 0, 1);
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
    if (!surface->closing_.load()) {
        QMetaObject::invokeMethod(surface, &VideoWidget::processUpdate, Qt::QueuedConnection);
    }
}

void VideoWidget::processUpdate() {
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
        // Wait for the double-click interval so a double click does not pause too.
        singleClickTimer_.start(QApplication::doubleClickInterval());
        event->accept();
    } else if (event->button() == Qt::MiddleButton) {
        emit middleClicked();
        event->accept();
    } else {
        QOpenGLWidget::mousePressEvent(event);
    }
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    singleClickTimer_.stop();
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked();
        event->accept();
    } else {
        QOpenGLWidget::mouseDoubleClickEvent(event);
    }
}

void VideoWidget::wheelEvent(QWheelEvent *event) {
    emit wheelScrolled(event->angleDelta().y());
    event->accept();
}
