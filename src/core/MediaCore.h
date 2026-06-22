/*

解封装线程 (DemuxLoop)：就像一个拆快递箱的工人。它专门负责从硬盘里的 MP4/MKV 文件中读取数据。遇到视频压缩包（Packet），就塞进视频队列；遇到音频压缩包，就塞进音频队列。
音频解码线程 (AudioDecodeLoop)：从音频队列里拿出 Packet，送进 FFmpeg 的音频解码器，解压出原始的声音帧（Frame），再塞进已解码的音频队列。
视频解码线程 (VideoDecodeLoop)：和音频一样，不过它处理的是视频画面。
视频渲染线程 (VideoRenderLoop)：这个工人拿着“时钟”，盯着视频解压好的帧。如果时间到了，就立刻把画面交给 IVideoRenderer 接口，让界面（QML）画到屏幕上。 (注：音频的播放通常交给系统的声卡驱动回调来要数据，所以我们后面会单独写一个 PulseAudio 引擎来负责音频播放，不需要手动写一个音频渲染线程)。

*/

#ifndef MEDIACORE_H
#define MEDIACORE_H

#include "MediaType.h"
#include "AudioEngine.h"

class PlayerController;

class MediaCore {
    friend class PlayerController;
public:
    explicit MediaCore();
    ~MediaCore();

    // 负责音视频处理的纯底层引擎 故使用std::string
    int Play(const std::string& url);
    void Stop();
    void SetPause(bool pause);
    void SetVideoRenderer(IVideoRenderer* renderder);

    // 用户通过 UI 调用的接口（传入秒数）
    void Seek(double seconds);

    // 获取当前播放进度（秒）
    double GetPosition() { return videoClock.Get(); }
    // 获取视频总长（秒）
    double GetDuration() {
        if (inputCtxPtr) {
            return static_cast<double>(inputCtxPtr->duration) / AV_TIME_BASE;
        }
        return 0.0;
    }

private:
    void DemuxLoop();          // 拆包线程：负责从文件读 AVPacket
    void AudioDecodeLoop();    // 音频解码线程：把 AVPacket 变成 AVFrame
    void VideoDecodeLoop();    // 视频解码线程：把 AVPacket 变成 AVFrame
    void VideoRenderLoop();    // 视频渲染线程：按时钟把图像送给 UI 画出来

    // FFmpeg 的总上下文（代表那个打开的视频文件）
    AVFormatContextPtr inputCtxPtr;

    // 分别管理自己的解码器和队列
    std::unique_ptr<Decoder> audioDecoder;
    std::unique_ptr<Decoder> videoDecoder;

    // 音频引擎（负责连接 Linux 的 PulseAudio 声卡发声）
    std::unique_ptr<AudioEngine> audioEngine;

    // 视频渲染器（负责让 UI 画图）
    IVideoRenderer* videoRenderer{nullptr};

    Clock audioClock{};
    Clock videoClock{};

    std::thread demuxThread;
    std::thread audioDecodeThread;
    std::thread videoDecodeThread;
    std::thread videoRenderThread;

    // 记录视频流和音频流在文件里的编号
    int audioStreamIndex{-1};
    int videoStreamIndex{-1};

    // 控制所有线程是否需要退出、是否正在暂停的全局标志位
    // 使用 std::atomic 是为了线程安全（多线程同时读写不会出错）
    std::atomic<bool> bStopDemux{false};
    std::atomic<bool> bPauseReq{true};

    std::atomic<bool> bSeekReq{false};         // 是否有跳转请求
    double seekTarget{0.0};                    // 跳转目标(s)
    std::atomic<bool> audioNeedFlush{false};   // 音频冲水标志
    std::atomic<bool> videoNeedFlush{false};   // 视频冲水标志
};

#endif