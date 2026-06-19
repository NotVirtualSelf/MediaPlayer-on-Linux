#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <pulse/simple.h>
#include <pulse/error.h>
#include <mutex>
#include <atomic>
#include <string>

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // 打开声卡，告诉它我们要播放的采样率和声道数
    bool Open(int sampleRate, int channels);
    void Close();

    // 将波形数据喂给声卡发声
    void Write(const void* data, size_t bytes);
    void SetPause(bool pause);

private:
    pa_simple* m_pa{nullptr};
    std::mutex m_mutex;
    std::atomic<bool> m_paused{false};
};

#endif