#include "VideoDisplay.h"
#include <QPainter>

extern "C" {
#include <libswscale/swscale.h>
}

VideoDisplay::VideoDisplay(QQuickItem* parent)
    : QQuickPaintedItem(parent) {
    // 告诉 Qt 这个控件不需要自己填充背景色，加快渲染速度
    setOpaquePainting(true);
}

VideoDisplay::~VideoDisplay() {
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
    }
}

void VideoDisplay::PrepareFrame(AVFrame* frame) {
    if (!frame) return;

    if (m_videoWidth != frame->width ||
        m_videoHeight != frame->height ||
        m_videoFormat != frame->format) {

        if (m_swsCtx) sws_freeContext(m_swsCtx);
        
        m_videoWidth = frame->width;
        m_videoHeight = frame->height;
        m_videoFormat = frame->format;

        // 让 FFmpeg 生成一个“转换器”
        // 目标格式：AV_PIX_FMT_RGB32 (即能在 Qt 中直接显示的 32位 RGBA)
        m_swsCtx = sws_getContext(
            m_videoWidth, m_videoHeight, (AVPixelFormat)m_videoFormat,
            m_videoWidth, m_videoHeight, AV_PIX_FMT_RGB32,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
    }

    if (!m_swsCtx) return;

    if (m_image.width() != m_videoWidth ||
        m_image.height() != m_videoHeight) {

        std::lock_guard<std::mutex> lock(m_mutex);
        m_image = QImage(m_videoWidth, m_videoHeight, QImage::Format_RGB32);
    }

    uint8_t* dest_data[4] { m_image.bits(), nullptr, nullptr, nullptr };
    int dest_linesize[4] { (int)m_image.bytesPerLine(), 0, 0, 0 };

    if (std::lock_guard<std::mutex> lock(m_mutex); true) {
        sws_scale(m_swsCtx,
                  frame->data, frame->linesize, 0, m_videoHeight,
                  dest_data, dest_linesize);
    }

    QMetaObject::invokeMethod(this, [this]() {
        this->update();
    }, Qt::QueuedConnection);
}

void VideoDisplay::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_image.fill(Qt::black);
    QMetaObject::invokeMethod(this, [this]() {
        this->update();
    }, Qt::QueuedConnection);
}

void VideoDisplay::paint(QPainter* painter) {
    std::lock_guard<std::mutex> lock(m_mutex);

    painter->fillRect(boundingRect(), Qt::black);

    if (m_image.isNull()) return;

    // 添加Qt::KeepAspectRatio 保证在不改变原图比例的前提下，尽量把图片放大/缩放，直到碰到窗口的某一条边为止。
    QImage scaledImg = m_image.scaled(boundingRect().size().toSize(),
                                        Qt::KeepAspectRatio, 
                                        Qt::SmoothTransformation);
    int x = ((boundingRect().width()) - scaledImg.width()) / 2;
    int y = ((boundingRect().height()) - scaledImg.height()) / 2;

    painter->drawImage(x, y, scaledImg);
}