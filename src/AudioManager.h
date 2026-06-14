#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <Windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

class AudioManager {
public:
    using CompletionCallback = std::function<void()>;

    AudioManager() = default;
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool Play(const std::string& filepath, bool async = false, CompletionCallback onComplete = nullptr);
    void Stop();
    bool EnqueueWav(const std::string& filepath);
    bool IsPlaying() const;
    float GetCurrentAmplitude() const;
    bool SetVolume(float volume);
    float GetVolume() const;
    int GetDuration(const std::string& filepath);

private:
    bool PlayWav(const std::string& filepath, bool async, CompletionCallback onComplete);
    bool PlayMp3(const std::string& filepath, bool async, CompletionCallback onComplete);

    bool PlayWavAsync(const std::string& filepath, CompletionCallback onComplete);
    static void CALLBACK WaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
                                      DWORD_PTR dwParam1, DWORD_PTR dwParam2);
    void OnWaveOutDone(WAVEHDR* pHdr);
    void CleanupWaveOut();
    float ComputeRmsAmplitude(const int16_t* samples, int count);

    bool PlayMp3Async(const std::string& filepath, CompletionCallback onComplete);
    static void Mp3WaitThreadProc(AudioManager* self, const std::string& filepath,
                                   CompletionCallback onComplete);

    std::atomic<bool> m_isPlaying{false};
    std::atomic<float> m_volume{1.0f};
    std::atomic<float> m_currentAmplitude{0.0f};
    std::thread m_playThread;
    CompletionCallback m_onComplete;

    std::vector<int16_t> m_wavPcmData;
    int m_wavSampleRate = 0;
    int m_wavChannels = 0;
    int m_wavBytesPerSample = 0;
    int m_wavCurrentByte = 0;
    bool m_wavDataReady = false;

    HWAVEOUT m_hWaveOut = nullptr;
    WAVEHDR m_waveHdr[2];
    bool m_doubleBuffer = false;

    std::atomic<bool> m_mp3Fallback{false};
    std::atomic<int> m_audioDurationMs{0};
    std::chrono::steady_clock::time_point m_audioStartTime;

    std::mutex m_queueMutex;
    std::vector<std::vector<int16_t>> m_pcmQueue;
    std::atomic<bool> m_queueActive{false};
};