#include "MediaCore.h"
#include <iostream>

extern "C" {
#include <libswresample/swresample.h>
}

MediaCore::MediaCore() {}
MediaCore::~MediaCore() {
    Stop();
}

void MediaCore::SetVideoRenderer(IVideoRenderer* renderer) {
    videoRenderer = renderer;
}

void MediaCore::Stop() {
    bStopDemux = true;
    bPauseReq = false;

    if (audioDecoder) {
        audioDecoder->pkt_queue.Wake();
        audioDecoder->frame_queue.Wake();
    }
    if (videoDecoder) {
        videoDecoder->pkt_queue.Wake();
        videoDecoder->frame_queue.Wake();
    }

    if (demuxThread.joinable()) demuxThread.join();
    if (audioDecodeThread.joinable()) audioDecodeThread.join();
    if (videoDecodeThread.joinable()) videoDecodeThread.join();
    if (videoRenderThread.joinable()) videoRenderThread.join();

    if (audioDecoder) audioDecoder->Reset();
    if (videoDecoder) videoDecoder->Reset();
    inputCtxPtr.reset();
}

int MediaCore::Play(const std::string& url) {
    Stop();
    bStopDemux = false;

    // 初始化 FFmpeg：打开文件
    AVFormatContext* formatCtx = nullptr;
    if (avformat_open_input(&formatCtx, url.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "open failure" << url << std::endl;
        return -1;
    }
    inputCtxPtr.reset(formatCtx);

    // 扫描文件找出流信息
    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        return -1;
    }

    videoStreamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audioStreamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    // 根据找到的轨道，配置相应的Decoder
    if (videoStreamIndex >= 0) {
        videoDecoder = std::make_unique<Decoder>();
        AVStream* vStream = formatCtx->streams[videoStreamIndex];
        const AVCodec* codec = avcodec_find_decoder(vStream->codecpar->codec_id);
        AVCodecContext* codecCtx = avcodec_alloc_context3(codec); // 申请内存
        avcodec_parameters_to_context(codecCtx, vStream->codecpar); // 提供视频参数
        avcodec_open2(codecCtx, codec, nullptr);

        videoDecoder->avctxPtr.reset(codecCtx);
    }

    if (audioStreamIndex >= 0) {
        audioDecoder = std::make_unique<Decoder>();
        AVStream* aStream = formatCtx->streams[audioStreamIndex];
        const AVCodec* codec = avcodec_find_decoder(aStream->codecpar->codec_id);
        AVCodecContext* codecCtx = avcodec_alloc_context3(codec); // 申请内存
        avcodec_parameters_to_context(codecCtx, aStream->codecpar); // 提供视频参数
        avcodec_open2(codecCtx, codec, nullptr);
        
        audioDecoder->avctxPtr.reset(codecCtx);
    }

    audioEngine = std::make_unique<AudioEngine>();

    videoClock.Reset();
    audioClock.Reset();

    demuxThread = std::thread(&MediaCore::DemuxLoop, this);
    videoDecodeThread = std::thread(&MediaCore::VideoDecodeLoop, this);
    audioDecodeThread = std::thread(&MediaCore::AudioDecodeLoop, this);
    videoRenderThread = std::thread(&MediaCore::VideoRenderLoop, this);

    bPauseReq = false;
    return 0;
}

void MediaCore::SetPause(bool pause) {
    bPauseReq = pause;
    if (audioEngine) {
        audioEngine->SetPause(pause);
    }

    // 如果是恢复播放，重置一下时钟的参考时间，避免快进
    if (!pause) {
        videoClock.Set(videoClock.Get(), 0);
    }
}

// 拆包
void MediaCore::DemuxLoop() {
    while (!bStopDemux) {
        // 让解压和后续的渲染线程挂起，不然一直解析和渲染会占据内存
        if (bPauseReq) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (bSeekReq) {
            int64_t targetTimeStamp = static_cast<int64_t>(seekTarget * AV_TIME_BASE);

            // FFmpeg 的视频不仅有完整的关键帧 (I帧)，还有只存了差异的增量帧 (P/B帧)
            // AVSEEK_FLAG_BACKWARD 表示：严格跳到目标时间之前的最近一个“关键帧”
            // 如果跳到了普通的 P 帧，因为缺少前面的画面作为参考会花屏
            if (av_seek_frame(inputCtxPtr.get(), -1, targetTimeStamp, AVSEEK_FLAG_BACKWARD) >= 0) {
                if (audioDecoder) audioDecoder->Flush();
                if (videoDecoder) videoDecoder->Flush();

                audioNeedFlush = true;
                videoNeedFlush = true;

                audioClock.Set(seekTarget, 0);
                videoClock.Set(seekTarget, 0);
            }

            bSeekReq = false;
            continue;
        }

        AVPacket* pkt = av_packet_alloc();

        if (av_read_frame(inputCtxPtr.get(), pkt) >= 0) {
            PacketUnit unit;
            unit.pkt.reset(pkt);
            unit.data_size = pkt->size;

            if (pkt->stream_index == videoStreamIndex) {
                videoDecoder->pkt_queue.Push(std::move(unit));
            } else if (pkt->stream_index == audioStreamIndex) {
                audioDecoder->pkt_queue.Push(std::move(unit));
            } else {}
        } else {
            av_packet_free(&pkt);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// 视频解码
void MediaCore::VideoDecodeLoop() {
    while (!bStopDemux) {
        PacketUnit unit;

        if (videoDecoder && videoDecoder->pkt_queue.Pop(unit, bStopDemux)) {
            if (videoNeedFlush) {
                // FFmpeg 的解码器是有记忆的
                // 内部会偷偷藏几张刚刚解压出来的历史画面，随时准备用来拼凑下一张画面。
                avcodec_flush_buffers(videoDecoder->avctxPtr.get());
                videoNeedFlush = false;
            }
            // 把压缩包，丢进 FFmpeg 解压
            avcodec_send_packet(videoDecoder->avctxPtr.get(), unit.pkt.get());

            // 尝试去机器出口拿解压好的图片。
            // 为什么要写死循环 (while)？因为一个包可能解出好几张图，也可能一张都没有，得拿到拿不出来为止。
            while (true) {
                AVFrame* frame = av_frame_alloc();
                if (avcodec_receive_frame(videoDecoder->avctxPtr.get(), frame) == 0) {
                    FrameUnit fUnit;
                    fUnit.frame.reset(frame);

                    AVStream* stream = inputCtxPtr->streams[videoStreamIndex];
                    fUnit.pts = frame->pts * av_q2d(stream->time_base);

                    videoDecoder->frame_queue.Push(std::move(fUnit));
                } else {
                    av_frame_free(&frame);
                    break;
                }
            }
        }
    }
}

// 音频解码
void MediaCore::AudioDecodeLoop() {
    SwrContext* swrCtx{nullptr};
    uint8_t* out_buffer{nullptr};

    while (!bStopDemux) {
        PacketUnit unit;

        if (audioDecoder && audioDecoder->pkt_queue.Pop(unit, bStopDemux)) {
            if (audioNeedFlush) {
                avcodec_flush_buffers(audioDecoder->avctxPtr.get());
                audioNeedFlush = false;
            }
            
            // 把压缩包，丢进 FFmpeg 解压
            avcodec_send_packet(audioDecoder->avctxPtr.get(), unit.pkt.get());

            while (true) {
                AVFrame* frame = av_frame_alloc();
                if (avcodec_receive_frame(audioDecoder->avctxPtr.get(), frame) == 0) {
                    if (!swrCtx) {
                        AVChannelLayout out_ch_layout;
                        av_channel_layout_default(&out_ch_layout, 2);

                        swr_alloc_set_opts2(
                            &swrCtx,
                            &out_ch_layout,
                            AV_SAMPLE_FMT_S16, // 目标16位整数格式
                            44100, // 目标44100Hz采样率
                            &frame->ch_layout, //原视频的声道布局
                            (AVSampleFormat)frame->format, // 原格式
                            frame->sample_rate, // 原采样率
                            0,
                            nullptr
                        );

                        swr_init(swrCtx);
                        av_channel_layout_uninit(&out_ch_layout);

                        if (audioEngine) {
                            audioEngine->Open(44100, 2);
                        }

                        out_buffer = (uint8_t*)av_malloc(44100 * 2 * 2);
                    }

                    int out_samples = swr_convert(
                        swrCtx,
                        &out_buffer,
                        44100,
                        (const uint8_t**)frame->data,
                        frame->nb_samples
                    );
                    int out_size = out_samples * 2 * 2;
                    if (audioEngine && out_size > 0) {
                        audioEngine->Write(out_buffer, out_size);
                    }

                    AVStream* stream = inputCtxPtr->streams[audioStreamIndex];
                    double pts = frame->pts * av_q2d(stream->time_base);
                    audioClock.Set(pts, 0);
                    videoClock.Set(pts, 0);

                    av_frame_free(&frame);
                } else {
                    av_frame_free(&frame);
                    break;
                }
            }
        }
    }
    
    if (swrCtx) swr_free(&swrCtx);
    if (out_buffer) av_freep(&out_buffer);
}

void MediaCore::VideoRenderLoop() {
    while (!bStopDemux) {
        FrameUnit unit;
        if (videoDecoder && videoDecoder->frame_queue.Pop(unit, bStopDemux)) {
            double current_clock = videoClock.Get();
            double delay = unit.pts - current_clock;
            if (delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay * 1000)));
            }
            if (videoRenderer) {
                videoRenderer->PrepareFrame(unit.frame.get());
            }
        }
    }
}

void MediaCore::Seek(double seconds) {
    seekTarget = seconds;
    bSeekReq = true;

    if (audioDecoder) {
        audioDecoder->pkt_queue.Wake();
        audioDecoder->frame_queue.Wake();
    }

    if (videoDecoder) {
        videoDecoder->pkt_queue.Wake();
        videoDecoder->frame_queue.Wake();
    }
}