#include "AudioEngine.h"
#include <iostream>

AudioEngine::AudioEngine() {}
AudioEngine::~AudioEngine() {
    Close();
}

bool AudioEngine::Open(int sampleRate, int channels) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pa) Close();

    // 强行规定我们要喂给声卡的数据格式：16位小端整数 (S16LE)
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.channels = channels;
    ss.rate = sampleRate;

    int error{-1};
    m_pa = pa_simple_new(
        nullptr, "MediaPlayer", PA_STREAM_PLAYBACK, nullptr, "Music",
        &ss, nullptr, nullptr, &error
    );

    if (!m_pa) {
        std::cerr << "PulseAudio open failed" << pa_strerror(error) << std::endl;
        return false;
    }

    return true;
}

void AudioEngine::Write(const void* data, size_t bytes) {
    if (m_paused) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pa) {
        int error{-1};
        if (pa_simple_write(m_pa, data, bytes, &error) < 0) {
            std::cerr << "PulseAudio write failed" << pa_strerror(error) << std::endl;
        }
    }
}

void AudioEngine::Close() {
    if (m_pa) {
        int error;
        pa_simple_drain(m_pa, &error); 
        pa_simple_free(m_pa);          
        m_pa = nullptr;
    }
}

void AudioEngine::SetPause(bool pause) {
    m_paused = pause;
}