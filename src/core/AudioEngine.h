#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <pulse/simple.h>
#include <pulse/error.h>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>

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

    // PulseAudio没有直接提供控制音量的函数，手动用数学乘法把波形压扁
    void SetVolume(float volume);

private:
    pa_simple* m_pa{nullptr};
    std::mutex m_mutex;
    std::atomic<bool> m_paused{false};

    float m_volume{1.0f};
    std::vector<uint8_t> m_volBuffer{};
};

#endif