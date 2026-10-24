#include "ffmpegthumbnaildecoder.h"

#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <limits>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace {
struct InterruptState {
    const std::shared_ptr<std::atomic_bool> *cancellation = nullptr;
};

int interruptCallback(void *opaque) {
    const auto *state = static_cast<const InterruptState *>(opaque);
    return state && state->cancellation && *state->cancellation &&
                   (*state->cancellation)->load(std::memory_order_relaxed)
               ? 1
               : 0;
}

bool cancelled(const ThumbnailRequest &request) {
    return request.cancellation && request.cancellation->load(std::memory_order_relaxed);
}

QString ffmpegError(int code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

QSize targetSize(int width, int height, const QSize &maximum) {
    const auto bounded = QSize(qMax(1, maximum.width()), qMax(1, maximum.height()));
    return QSize(width, height).scaled(bounded, Qt::KeepAspectRatio);
}

QImage convertFrame(const AVFrame *frame, const QSize &maximum, QString *error) {
    if (!frame || frame->width <= 0 || frame->height <= 0 || frame->format < 0) {
        *error = QStringLiteral("video frame has invalid dimensions or pixel format");
        return {};
    }
    const auto output = targetSize(frame->width, frame->height, maximum);
    QImage image(output, QImage::Format_RGB888);
    image.fill(Qt::black);
    auto *scale = sws_getContext(
        frame->width, frame->height, static_cast<AVPixelFormat>(frame->format), output.width(),
        output.height(), AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!scale) {
        *error = QStringLiteral("could not create FFmpeg scaler");
        return {};
    }
    uint8_t *destination[] = {image.bits()};
    int destinationStride[] = {static_cast<int>(image.bytesPerLine())};
    const auto scaledLines = sws_scale(scale, frame->data, frame->linesize, 0, frame->height,
                                       destination, destinationStride);
    sws_freeContext(scale);
    if (scaledLines != output.height()) {
        *error = QStringLiteral("could not convert FFmpeg video frame");
        return {};
    }
    return image;
}
} // namespace

ThumbnailResult decodeThumbnail(const ThumbnailRequest &request) {
    ThumbnailResult result;
    if (request.source.isEmpty() || !QFileInfo::exists(request.source)) {
        result.error = QStringLiteral("thumbnail source does not exist");
        return result;
    }
    if (cancelled(request)) {
        result.error = QStringLiteral("thumbnail request cancelled");
        return result;
    }
    if (!std::isfinite(request.seconds) || request.seconds < 0) {
        result.error = QStringLiteral("thumbnail timestamp is invalid");
        return result;
    }

    AVFormatContext *format = avformat_alloc_context();
    if (!format) {
        result.error = QStringLiteral("could not allocate FFmpeg format context");
        return result;
    }
    const InterruptState interrupt{&request.cancellation};
    format->interrupt_callback = {interruptCallback, const_cast<InterruptState *>(&interrupt)};
    const auto source = request.source.toUtf8();
    int error = avformat_open_input(&format, source.constData(), nullptr, nullptr);
    if (error < 0) {
        result.error = ffmpegError(error);
        avformat_close_input(&format);
        return result;
    }
    error = avformat_find_stream_info(format, nullptr);
    if (error < 0) {
        result.error = ffmpegError(error);
        avformat_close_input(&format);
        return result;
    }

    const auto streamIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        result.error = QStringLiteral("media has no video stream");
        avformat_close_input(&format);
        return result;
    }
    auto *stream = format->streams[streamIndex];
    const auto *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        result.error = QStringLiteral("video decoder is unavailable");
        avformat_close_input(&format);
        return result;
    }
    AVCodecContext *decoder = avcodec_alloc_context3(codec);
    if (!decoder) {
        result.error = QStringLiteral("could not allocate video decoder");
        avformat_close_input(&format);
        return result;
    }
    error = avcodec_parameters_to_context(decoder, stream->codecpar);
    if (error >= 0)
        error = avcodec_open2(decoder, codec, nullptr);
    if (error < 0) {
        result.error = ffmpegError(error);
        avcodec_free_context(&decoder);
        avformat_close_input(&format);
        return result;
    }

    constexpr auto maximumSeconds =
        static_cast<double>(std::numeric_limits<int64_t>::max()) / AV_TIME_BASE;
    if (request.seconds > maximumSeconds) {
        result.error = QStringLiteral("thumbnail timestamp is too large");
        avcodec_free_context(&decoder);
        avformat_close_input(&format);
        return result;
    }
    const auto streamStart = stream->start_time == AV_NOPTS_VALUE ? 0 : stream->start_time;
    const auto relativeTimestamp =
        av_rescale_q(static_cast<int64_t>(request.seconds * AV_TIME_BASE),
                     AVRational{1, AV_TIME_BASE}, stream->time_base);
    if (relativeTimestamp > 0 &&
        streamStart > std::numeric_limits<int64_t>::max() - relativeTimestamp) {
        result.error = QStringLiteral("thumbnail timestamp is too large");
        avcodec_free_context(&decoder);
        avformat_close_input(&format);
        return result;
    }
    const auto timestamp = streamStart + relativeTimestamp;
    error = av_seek_frame(format, streamIndex, timestamp, AVSEEK_FLAG_BACKWARD);
    if (error < 0) {
        result.error = ffmpegError(error);
        avcodec_free_context(&decoder);
        avformat_close_input(&format);
        return result;
    }
    avcodec_flush_buffers(decoder);

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    bool decoded = false;
    bool reachedTarget = false;
    double candidateTimestamp = 0.0;
    QImage candidateImage;
    const auto considerFrame = [&] {
        const auto timeBaseSeconds = av_q2d(stream->time_base);
        const auto frameTimestamp =
            frame->best_effort_timestamp == AV_NOPTS_VALUE
                ? request.seconds
                : (frame->best_effort_timestamp * timeBaseSeconds - streamStart * timeBaseSeconds);
        const bool isCloser = !decoded || std::abs(frameTimestamp - request.seconds) <
                                              std::abs(candidateTimestamp - request.seconds);
        if (isCloser) {
            auto image = convertFrame(frame, request.maximumSize, &result.error);
            if (!image.isNull()) {
                candidateImage = std::move(image);
                candidateTimestamp = frameTimestamp;
                decoded = true;
            }
        }
        reachedTarget = frameTimestamp >= request.seconds;
    };
    const auto receiveFrames = [&] {
        int receiveError = 0;
        while (!cancelled(request) && !reachedTarget &&
               (receiveError = avcodec_receive_frame(decoder, frame)) >= 0)
            considerFrame();
        return receiveError;
    };
    if (!packet || !frame) {
        result.error = QStringLiteral("could not allocate FFmpeg frame buffers");
    } else {
        while (!cancelled(request) && !reachedTarget && av_read_frame(format, packet) >= 0) {
            if (packet->stream_index != streamIndex) {
                av_packet_unref(packet);
                continue;
            }
            do {
                error = avcodec_send_packet(decoder, packet);
                if (error == AVERROR(EAGAIN))
                    receiveFrames();
            } while (error == AVERROR(EAGAIN) && !cancelled(request) && !reachedTarget);
            av_packet_unref(packet);
            if (error < 0)
                break;
            receiveFrames();
        }
        if (!cancelled(request) && !reachedTarget) {
            error = avcodec_send_packet(decoder, nullptr);
            if (error >= 0 || error == AVERROR_EOF)
                receiveFrames();
        }
        if (decoded)
            result.image = std::move(candidateImage), result.timestamp = candidateTimestamp;
        else if (result.error.isEmpty())
            result.error = cancelled(request) ? QStringLiteral("thumbnail request cancelled")
                                              : QStringLiteral("could not decode video frame");
    }
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&decoder);
    avformat_close_input(&format);
    return result;
}
