#include "AudioManager.h"
#include "utils.h"
#include "LAppPal.h"
#include <algorithm>
#include <cmath>

AudioManager::~AudioManager() {
    Stop();
}

bool AudioManager::Play(const std::string& filepath, bool async, CompletionCallback onComplete) {
    LAppPal::PrintLog("[AudioManager] Play: %s", filepath.c_str());

    if (m_isPlaying.load()) {
        Stop();
    }

    if (!utils::FileExists(filepath)) {
        return false;
    }

    m_onComplete = onComplete;

    std::string ext;
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = filepath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == ".wav") {
        return PlayWav(filepath, async, onComplete);
    } else if (ext == ".mp3") {
        return PlayMp3(filepath, async, onComplete);
    }

    return false;
}

void AudioManager::Stop() {
    m_isPlaying.store(false);
    m_currentAmplitude.store(0.0f);
    m_mp3Fallback.store(false);
    m_queueActive.store(false);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pcmQueue.clear();
    }

    CleanupWaveOut();

    if (m_playThread.joinable()) {
        m_playThread.join();
    }

    m_wavPcmData.clear();
    m_wavDataReady = false;
}

bool AudioManager::IsPlaying() const {
    return m_isPlaying.load();
}

float AudioManager::GetCurrentAmplitude() const {
    if (m_mp3Fallback.load()) {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float, std::milli>(now - m_audioStartTime).count();
        int duration = m_audioDurationMs.load();
        if (duration <= 0) return 0.0f;
        float progress = elapsed / static_cast<float>(duration);
        if (progress > 1.0f) progress = 1.0f;
        float envelope = progress < 0.1f ? progress / 0.1f :
                         progress > 0.9f ? (1.0f - progress) / 0.1f : 1.0f;
        return fabsf(sinf(progress * 30.0f)) * envelope * 0.7f;
    }
    return m_currentAmplitude.load();
}

bool AudioManager::SetVolume(float volume) {
    volume = std::max(0.0f, std::min(1.0f, volume));
    m_volume.store(volume);

    DWORD volValue = static_cast<DWORD>(volume * 0xFFFF);
    DWORD dwVolume = volValue | (volValue << 16);
    waveOutSetVolume(nullptr, dwVolume);

    return true;
}

float AudioManager::GetVolume() const {
    return m_volume.load();
}

int AudioManager::GetDuration(const std::string& filepath) {
    if (!utils::FileExists(filepath)) {
        return -1;
    }

    std::string ext;
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = filepath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == ".wav") {
        std::vector<uint8_t> data = utils::ReadFileToBytes(filepath);
        if (data.size() < 44) return -1;

        int sampleRate = *reinterpret_cast<int*>(&data[24]);
        int byteRate = *reinterpret_cast<int*>(&data[28]);
        int dataSize = *reinterpret_cast<int*>(&data[40]);

        if (byteRate > 0) {
            return (dataSize * 1000) / byteRate;
        }
        return -1;
    }

    if (ext == ".mp3") {
        std::wstring wpath = utils::Utf8ToWide(filepath);
        wchar_t buffer[256] = {0};
        mciSendStringW((L"open \"" + wpath + L"\" type mpegvideo alias mp3_duration").c_str(), nullptr, 0, nullptr);
        mciSendStringW(L"status mp3_duration length", buffer, 256, nullptr);
        mciSendStringW(L"close mp3_duration", nullptr, 0, nullptr);
        return _wtoi(buffer);
    }

    return -1;
}

bool AudioManager::EnqueueWav(const std::string& filepath) {
    std::vector<uint8_t> fileData = utils::ReadFileToBytes(filepath);
    if (fileData.size() < 44) return false;

    int audioFormat = *reinterpret_cast<int16_t*>(&fileData[20]);
    int16_t channels = *reinterpret_cast<int16_t*>(&fileData[22]);
    int32_t sampleRate = *reinterpret_cast<int32_t*>(&fileData[24]);
    int16_t bitsPerSample = *reinterpret_cast<int16_t*>(&fileData[34]);
    int dataSize = *reinterpret_cast<int32_t*>(&fileData[40]);

    if (audioFormat != 1) return false;

    int bytesPerSample = (bitsPerSample / 8) * channels;
    int sampleCount = dataSize / bytesPerSample;

    std::vector<int16_t> pcmData(sampleCount);
    memcpy(pcmData.data(), fileData.data() + 44, dataSize);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pcmQueue.push_back(std::move(pcmData));
    }

    m_queueActive.store(true);
    return true;
}

bool AudioManager::PlayWav(const std::string& filepath, bool async, CompletionCallback onComplete) {
    if (async) {
        return PlayWavAsync(filepath, onComplete);
    }

    m_isPlaying.store(true);
    std::wstring widePath = utils::Utf8ToWide(filepath);
    MMRESULT result = PlaySoundW(widePath.c_str(), nullptr, SND_FILENAME | SND_SYNC);
    m_isPlaying.store(false);

    if (onComplete) {
        onComplete();
    }

    return result == MMSYSERR_NOERROR;
}

bool AudioManager::PlayMp3(const std::string& filepath, bool async, CompletionCallback onComplete) {
    if (async) {
        return PlayMp3Async(filepath, onComplete);
    }

    std::wstring widePath = utils::Utf8ToWide(filepath);
    std::wstring openCmd = L"open \"" + widePath + L"\" type mpegvideo alias mp3_player";
    std::wstring playCmd = L"play mp3_player wait";
    std::wstring closeCmd = L"close mp3_player";

    MCIERROR err = mciSendStringW(openCmd.c_str(), nullptr, 0, nullptr);
    if (err != 0) {
        return false;
    }

    m_isPlaying.store(true);

    err = mciSendStringW(playCmd.c_str(), nullptr, 0, nullptr);
    if (err != 0) {
        mciSendStringW(closeCmd.c_str(), nullptr, 0, nullptr);
        m_isPlaying.store(false);
        return false;
    }

    mciSendStringW(closeCmd.c_str(), nullptr, 0, nullptr);
    m_isPlaying.store(false);

    if (onComplete) {
        onComplete();
    }

    return true;
}

void AudioManager::CleanupWaveOut() {
    if (m_hWaveOut) {
        waveOutReset(m_hWaveOut);
        for (int i = 0; i < 2; i++) {
            if (m_waveHdr[i].lpData) {
                waveOutUnprepareHeader(m_hWaveOut, &m_waveHdr[i], sizeof(WAVEHDR));
                m_waveHdr[i].lpData = nullptr;
            }
        }
        waveOutClose(m_hWaveOut);
        m_hWaveOut = nullptr;
    }
    m_doubleBuffer = false;
}

float AudioManager::ComputeRmsAmplitude(const int16_t* samples, int count) {
    if (count <= 0) return 0.0f;
    double sumSq = 0.0;
    for (int i = 0; i < count; i++) {
        double sample = static_cast<double>(samples[i]) / 32768.0;
        sumSq += sample * sample;
    }
    float rms = static_cast<float>(std::sqrt(sumSq / count));
    float normalized = rms * 3.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    return normalized;
}

void CALLBACK AudioManager::WaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
                                         DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (uMsg == WOM_DONE) {
        AudioManager* self = reinterpret_cast<AudioManager*>(dwInstance);
        WAVEHDR* pHdr = reinterpret_cast<WAVEHDR*>(dwParam1);
        self->OnWaveOutDone(pHdr);
    }
}

void AudioManager::OnWaveOutDone(WAVEHDR* pHdr) {
    if (!m_isPlaying.load() || !m_wavDataReady) return;

    const int16_t* samples = reinterpret_cast<const int16_t*>(pHdr->lpData);
    int sampleCount = pHdr->dwBufferLength / m_wavBytesPerSample;
    float amp = ComputeRmsAmplitude(samples, sampleCount);
    m_currentAmplitude.store(amp);

    int totalBytes = static_cast<int>(m_wavPcmData.size() * m_wavBytesPerSample);
    int remainingBytes = totalBytes - m_wavCurrentByte;

    if (remainingBytes <= 0) {
        if (m_queueActive.load()) {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (!m_pcmQueue.empty()) {
                std::vector<int16_t> nextChunk = std::move(m_pcmQueue.front());
                m_pcmQueue.erase(m_pcmQueue.begin());
                m_wavPcmData = std::move(nextChunk);
                m_wavCurrentByte = 0;
                m_wavDataReady = true;
                totalBytes = static_cast<int>(m_wavPcmData.size() * m_wavBytesPerSample);
                remainingBytes = totalBytes;
                if (m_pcmQueue.empty()) {
                    m_queueActive.store(false);
                }
            }
        }
    }

    if (remainingBytes <= 0) {
        waveOutUnprepareHeader(m_hWaveOut, pHdr, sizeof(WAVEHDR));
        pHdr->lpData = nullptr;
        CleanupWaveOut();
        m_isPlaying.store(false);
        m_currentAmplitude.store(0.0f);
        if (m_onComplete) {
            m_onComplete();
        }
        return;
    }

    int blockSize = m_wavSampleRate * m_wavBytesPerSample / 25;
    if (blockSize > remainingBytes) blockSize = remainingBytes;
    if (blockSize <= 0) {
        waveOutUnprepareHeader(m_hWaveOut, pHdr, sizeof(WAVEHDR));
        pHdr->lpData = nullptr;
        CleanupWaveOut();
        m_isPlaying.store(false);
        m_currentAmplitude.store(0.0f);
        if (m_onComplete) {
            m_onComplete();
        }
        return;
    }

    pHdr->lpData = reinterpret_cast<LPSTR>(m_wavPcmData.data() + m_wavCurrentByte / m_wavBytesPerSample);
    pHdr->dwBufferLength = blockSize;
    pHdr->dwFlags = 0;
    MMRESULT result = waveOutPrepareHeader(m_hWaveOut, pHdr, sizeof(WAVEHDR));
    if (result == MMSYSERR_NOERROR) {
        m_wavCurrentByte += blockSize;
        waveOutWrite(m_hWaveOut, pHdr, sizeof(WAVEHDR));
    }
}

bool AudioManager::PlayWavAsync(const std::string& filepath, CompletionCallback onComplete) {
    CleanupWaveOut();
    m_wavPcmData.clear();
    m_wavDataReady = false;
    m_currentAmplitude.store(0.0f);

    std::vector<uint8_t> fileData = utils::ReadFileToBytes(filepath);
    if (fileData.size() < 44) return false;

    int audioFormat = *reinterpret_cast<int16_t*>(&fileData[20]);
    m_wavChannels = *reinterpret_cast<int16_t*>(&fileData[22]);
    m_wavSampleRate = *reinterpret_cast<int32_t*>(&fileData[24]);
    int bitsPerSample = *reinterpret_cast<int16_t*>(&fileData[34]);
    int dataSize = *reinterpret_cast<int32_t*>(&fileData[40]);

    if (audioFormat != 1) {
        LAppPal::PrintLog("[AudioManager] Only PCM WAV supported for async lip-sync, format=%d", audioFormat);
        return false;
    }

    LAppPal::PrintLog("[AudioManager] WAV: %dHz %dch %dbit, data=%d bytes",
                      m_wavSampleRate, m_wavChannels, bitsPerSample, dataSize);

    m_wavBytesPerSample = (bitsPerSample / 8) * m_wavChannels;

    const uint8_t* pcmStart = fileData.data() + 44;
    int sampleCount = dataSize / m_wavBytesPerSample;
    m_wavPcmData.resize(sampleCount);
    memcpy(m_wavPcmData.data(), pcmStart, dataSize);
    m_wavDataReady = true;
    m_wavCurrentByte = 0;

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = static_cast<WORD>(m_wavChannels);
    wfx.nSamplesPerSec = m_wavSampleRate;
    wfx.wBitsPerSample = static_cast<WORD>(bitsPerSample);
    wfx.nBlockAlign = static_cast<WORD>(m_wavBytesPerSample);
    wfx.nAvgBytesPerSec = m_wavSampleRate * m_wavBytesPerSample;
    wfx.cbSize = 0;

    MMRESULT result = waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &wfx,
                                   reinterpret_cast<DWORD_PTR>(WaveOutProc),
                                   reinterpret_cast<DWORD_PTR>(this),
                                   CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        LAppPal::PrintLog("[AudioManager] waveOutOpen failed: %d", result);
        m_wavDataReady = false;
        return false;
    }

    int totalBytes = static_cast<int>(m_wavPcmData.size() * m_wavBytesPerSample);
    int blockSize = m_wavSampleRate * m_wavBytesPerSample / 25;

    int bytesQueued = 0;
    for (int i = 0; i < 2; i++) {
        int size = blockSize;
        if (bytesQueued + size > totalBytes) size = totalBytes - bytesQueued;
        if (size <= 0) break;

        memset(&m_waveHdr[i], 0, sizeof(WAVEHDR));
        m_waveHdr[i].lpData = reinterpret_cast<LPSTR>(m_wavPcmData.data() + bytesQueued / m_wavBytesPerSample);
        m_waveHdr[i].dwBufferLength = size;
        m_waveHdr[i].dwFlags = 0;

        result = waveOutPrepareHeader(m_hWaveOut, &m_waveHdr[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            CleanupWaveOut();
            m_wavDataReady = false;
            return false;
        }

        bytesQueued += size;
        result = waveOutWrite(m_hWaveOut, &m_waveHdr[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            CleanupWaveOut();
            m_wavDataReady = false;
            return false;
        }
    }

    m_wavCurrentByte = bytesQueued;
    m_doubleBuffer = true;
    m_isPlaying.store(true);
    return true;
}

bool AudioManager::PlayMp3Async(const std::string& filepath, CompletionCallback onComplete) {
    int durationMs = GetDuration(filepath);
    m_audioDurationMs.store(durationMs > 0 ? durationMs : 3000);
    m_audioStartTime = std::chrono::steady_clock::now();
    m_mp3Fallback.store(true);
    m_currentAmplitude.store(0.0f);

    m_playThread = std::thread(&AudioManager::Mp3WaitThreadProc, this, filepath, onComplete);
    m_isPlaying.store(true);
    return true;
}

void AudioManager::Mp3WaitThreadProc(AudioManager* self, const std::string& filepath,
                                      CompletionCallback onComplete) {
    std::wstring widePath = utils::Utf8ToWide(filepath);
    std::wstring openCmd = L"open \"" + widePath + L"\" type mpegvideo alias mp3_async";
    std::wstring playCmd = L"play mp3_async wait";
    std::wstring closeCmd = L"close mp3_async";

    MCIERROR err = mciSendStringW(openCmd.c_str(), nullptr, 0, nullptr);
    if (err != 0) {
        self->m_isPlaying.store(false);
        self->m_mp3Fallback.store(false);
        self->m_currentAmplitude.store(0.0f);
        return;
    }

    mciSendStringW(playCmd.c_str(), nullptr, 0, nullptr);
    mciSendStringW(closeCmd.c_str(), nullptr, 0, nullptr);

    self->m_isPlaying.store(false);
    self->m_mp3Fallback.store(false);
    self->m_currentAmplitude.store(0.0f);

    if (onComplete) {
        onComplete();
    }
}