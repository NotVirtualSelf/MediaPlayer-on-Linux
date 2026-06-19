#ifndef VIDEODISPLAY_H
#define VIDEODISPLAY_H

#include <QQuickPaintedItem>
#include <QImage>
#include <mutex>
#include "../core/MediaType.h"

// FFmpeg 专门用来做图像格式转换（比如 YUV 转 RGB）和缩放的工具
struct SwsContext;

// 继承 QQuickPaintedItem 让我们可以在 QML 里像写矩形一样使用它
// 继承 IVideoRenderer 是为了接收核心引擎 MediaCore 发来的图像数据
class VideoDisplay : public QQuickPaintedItem, public IVideoRenderer {
    Q_OBJECT
    QML_ELEMENT
    // 加上这行宏，QML 里能直接用 <VideoDisplay> 这个标签

public:
    explicit VideoDisplay(QQuickItem* parent = nullptr);
    ~VideoDisplay() override;

    void PrepareFrame(AVFrame* frame) override;
    void Clear() override;

protected:
    void paint(QPainter* painter) override;

private:
    QImage m_image; // 用来充当画布
    std::mutex m_mutex;

    SwsContext* m_swsCtx{nullptr};
    int m_videoWidth{0};
    int m_videoHeight{0};
    int m_videoFormat{-1};
};

#endif