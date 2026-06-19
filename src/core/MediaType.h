#ifndef BASE_H
#define BASE_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>

template <typename T, void (Func)(T**)>
struct AVDeleter {
    void operator()(T* p) const {
        if (p) Func(&p);
    }
};

using AVPacketPtr = std::unique_ptr<AVPacket, AVDeleter<AVPacket, av_packet_free>>;
using AVFramePtr = std::unique_ptr<AVFrame, AVDeleter<AVFrame, av_frame_free>>;
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVDeleter<AVCodecContext, avcodec_free_context>>;
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVDeleter<AVFormatContext, avformat_close_input>>;

// AVPacket (压缩包) 是 FFmpeg 原生的，我们需要给它贴上一些快递单号信息，变成 PacketUnit
struct PacketUnit {
    AVPacketPtr pkt;
    size_t data_size{0};
    int serial{0};
};

// AVFrame (原始帧) 也要被包装成 FrameUnit
struct FrameUnit {
    AVFramePtr frame{nullptr};
    double pts{0.0};
    double duration{0.0};
    size_t data_size{0};
    double speed{1.0};
    int serial{0};

    FrameUnit() = default;
    FrameUnit(FrameUnit&&) = default;
    FrameUnit& operator=(FrameUnit&&) = default;
    FrameUnit(const FrameUnit&) = delete;
    FrameUnit& operator=(const FrameUnit&) = delete;
};

template <typename T>
class Queue {
public:
    void Push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex);
        mem_size += item.data_size;
        queue.push_back(std::move(item));
        cv.notify_one();
    }

    // 阻塞式拿数据
    bool Pop(T& item, std::atomic<bool>& abort) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return !queue.empty() || abort.load(); });

        if (abort.load() || queue.empty()) return false;

        item = std::move(queue.front());
        mem_size -= item.data_size;
        queue.pop_front();
        return true;
    }

    // 非阻塞式拿数据
    bool TryPop(T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) return false;
        item = std::move(queue.front());
        mem_size -= item.data_size;
        queue.pop_front();
        return true;
    }

    void Flush() {
        std::lock_guard<std::mutex> lock(mutex);
        queue.clear();
        mem_size = 0;
        Wake();
    }

    void Wake() { cv.notify_all(); }
    size_t Size() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }
    size_t MemSize() {
        std::lock_guard<std::mutex> lock(mutex);
        return mem_size;
    }

private:
    std::mutex mutex;
    std::deque<T> queue;
    size_t mem_size{0};
    std::condition_variable cv;
};

// 音视频同步
// 因为视频和音频是两个完全不同的解码和播放线程。
// 常常出现音频播得比视频快，就会画面声音对不上（音画不同步）。
// 我们通常以“音频”为主时钟。视频每画一帧前，都要问一下时钟：“现在到几秒了？我的 PTS 到了吗？”
// 到了就画，没到就等，过了就直接丢弃这一帧赶紧追。
class Clock {
public:
    Clock() { Reset(); }

    void Reset() {
        std::lock_guard<std::mutex> lock(mutex);
        pts = 0.0;
        last_updated_time = av_gettime_relative() / trans;
        serial = 0;
    }

    void Set(double pts, int frameSerial) {
        std::lock_guard<std::mutex> lock(mutex);
        this->pts = pts;
        this->serial = frameSerial;
        last_updated_time = av_gettime_relative() / trans;
    }

    // 获取当前时钟的时间
    double Get() const {
        std::lock_guard<std::mutex> lock(mutex);
        double now = av_gettime_relative() / trans;
        return pts + (now - last_updated_time);
    }

private:
    double pts{0.0};
    double last_updated_time{0.0};
    int serial{0};
    mutable std::mutex mutex;
    static constexpr double trans{1000000.0};
};

// 管理属于自己轨道的上下文（AVCodecContext）
// 待解码队列（塞满了 Packet）
// 已解码队列（塞满了 Frame）
struct Decoder {
    AVCodecContextPtr avctxPtr{nullptr};
    Queue<PacketUnit> pkt_queue{};
    Queue<FrameUnit> frame_queue{};
    std::atomic<bool> bFinished{false};

    void Flush() {
        pkt_queue.Flush();
        frame_queue.Flush();
        bFinished = false;
    }

    void Reset() {
        Flush();
        avctxPtr.reset();
    }
};

// 视频渲染器接口：把画图的权利交接给 QML
// 只要 UI 层继承了这个接口，提供了一个 PrepareFrame 函数，核心引擎只要把解压好的图片(AVFrame)塞给它，它就能在屏幕上画出来。
class IVideoRenderer {
public:
    virtual ~IVideoRenderer() = default;
    virtual void PrepareFrame(AVFrame* frame) = 0;
    virtual void Clear() = 0;
};

#endif