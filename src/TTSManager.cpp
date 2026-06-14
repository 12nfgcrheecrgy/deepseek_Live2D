#include "TTSManager.h"
#include "LAppPal.h"
#include "utils.h"
#include "nlohmann/json.hpp"
#include <sstream>
#include <fstream>
#include <chrono>
#include <thread>

TTSManager::~TTSManager() {
    TerminateLoaderGUI();
    ShutdownIndexTTS2();
    ShutdownSoVITS();
}

void TTSManager::SetConfig(const std::string& voice, const std::string& rate, const std::string& pitch,
                            const std::string& voiceHappy, const std::string& voiceSurprised, const std::string& voiceNormal,
                            const std::string& rateHappy, const std::string& rateSurprised,
                            const std::string& pitchHappy, const std::string& pitchSurprised) {
    m_voice = voice;
    m_rate = rate;
    m_pitch = pitch;
    m_voiceHappy = voiceHappy;
    m_voiceSurprised = voiceSurprised;
    m_voiceNormal = voiceNormal;
    m_rateHappy = rateHappy;
    m_rateSurprised = rateSurprised;
    m_pitchHappy = pitchHappy;
    m_pitchSurprised = pitchSurprised;

    std::string exeDir = utils::GetCurrentExecutableDir();
    m_edgeScriptPath = exeDir + "\\python\\tts_helper.py";
    m_indexTts2ScriptPath = exeDir + "\\python\\index_tts2_bridge.py";
    m_ttsServerScriptPath = exeDir + "\\python\\tts_server.py";
    m_sovitsScriptPath = exeDir + "\\python\\sovits_server.py";
    m_loaderGuiPath = exeDir + "\\python\\model_loader_gui.py";
}

void TTSManager::SetIndexTTS2Config(const std::string& modelDir, const std::string& configPath,
                                     const std::string& referenceAudio, bool useFp16, bool useOpenVino,
                                     const std::string& openvinoDevice) {
    m_indexTts2ModelDir = modelDir;
    m_indexTts2ConfigPath = configPath;
    m_indexTts2ReferenceAudio = referenceAudio;
    m_indexTts2UseFp16 = useFp16;
    m_indexTts2UseOpenVino = useOpenVino;
    m_indexTts2OpenvinoDevice = openvinoDevice;
}

void TTSManager::SetSoVITSConfig(const std::string& modelPath, const std::string& configPath,
                                  const std::string& diffusionPath, const std::string& diffusionConfig,
                                  const std::string& referenceAudio, bool useFp16,
                                  const std::string& gptWeights) {
    m_sovitsModelPath = modelPath;
    m_sovitsConfigPath = configPath;
    m_sovitsDiffusionPath = diffusionPath;
    m_sovitsDiffusionConfig = diffusionConfig;
    m_sovitsReferenceAudio = referenceAudio;
    m_sovitsUseFp16 = useFp16;
    m_sovitsGptWeights = gptWeights;
}

bool TTSManager::StartLoaderGUI(const std::string& engineName) {
    if (!utils::FileExists(m_loaderGuiPath)) {
        LAppPal::PrintLog("[TTSManager] model_loader_gui.py not found: %s", m_loaderGuiPath.c_str());
        return false;
    }

    // Generate a unique progress file path
    std::string tempDir = utils::GetTempDirectory();
    std::string timestamp = utils::GetCurrentTimestamp();
    for (auto& c : timestamp) {
        if (c == ':' || c == '.' || c == ' ') c = '_';
    }
    m_progressFilePath = tempDir + "tts_progress_" + timestamp + ".json";

    // Write initial progress
    try {
        nlohmann::json j;
        j["percent"] = 0;
        j["message"] = "正在启动...";
        j["done"] = false;
        std::ofstream f(m_progressFilePath);
        f << j.dump();
    } catch (...) {}

    std::string pythonExe = PythonBridge::GetPythonExe();
    std::wstring cmdLine = L"\"" + utils::Utf8ToWide(pythonExe) + L"\" \""
                         + utils::Utf8ToWide(m_loaderGuiPath) + L"\""
                         + L" --progress-file \"" + utils::Utf8ToWide(m_progressFilePath) + L"\""
                         + L" --engine \"" + utils::Utf8ToWide(engineName) + L"\"";

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(L'\0');

    BOOL success = CreateProcessW(
        nullptr,
        cmdLineBuf.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (!success) {
        LAppPal::PrintLog("[TTSManager] Failed to start loader GUI");
        return false;
    }

    CloseHandle(pi.hThread);
    m_hLoaderGuiProcess = pi.hProcess;
    LAppPal::PrintLog("[TTSManager] Loader GUI started (PID: %lu)", pi.dwProcessId);
    return true;
}

void TTSManager::TerminateLoaderGUI() {
    if (m_hLoaderGuiProcess) {
        LAppPal::PrintLog("[TTSManager] Terminating loader GUI...");
        TerminateProcess(m_hLoaderGuiProcess, 0);
        CloseHandle(m_hLoaderGuiProcess);
        m_hLoaderGuiProcess = nullptr;
    }
    if (!m_progressFilePath.empty()) {
        DeleteFileW(utils::Utf8ToWide(m_progressFilePath).c_str());
        m_progressFilePath.clear();
    }
}

bool TTSManager::WaitForServerReady(int timeoutSec) {
    int timeoutMs = timeoutSec * 1000;
    auto start = std::chrono::high_resolution_clock::now();

    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed > timeoutMs) {
            LAppPal::PrintLog("[TTSManager] WaitForServerReady timeout (%d sec)", timeoutSec);
            return false;
        }

        // Check if bridge is ready
        if (m_bridge.IsPersistentReady()) {
            LAppPal::PrintLog("[TTSManager] Server is ready after %.1f sec", elapsed / 1000.0f);
            return true;
        }

        // Check if process died
        if (!m_bridge.IsPersistentAlive()) {
            LAppPal::PrintLog("[TTSManager] Server process died during startup");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

bool TTSManager::InitializeIndexTTS2(int timeoutSec, bool showGui) {
    LAppPal::PrintLog("[TTSManager] InitializeIndexTTS2 starting...");

    if (m_ttsServerReady) {
        LAppPal::PrintLog("[TTSManager] IndexTTS2 already initialized");
        return true;
    }

    if (!utils::FileExists(m_ttsServerScriptPath)) {
        LAppPal::PrintLog("[TTSManager] tts_server.py not found: %s", m_ttsServerScriptPath.c_str());
        return false;
    }

    // Set progress file environment variable
    if (showGui && !m_progressFilePath.empty()) {
        SetEnvironmentVariableW(L"TTS_PROGRESS_FILE", utils::Utf8ToWide(m_progressFilePath).c_str());
    }

    m_serverStarting = true;
    int timeoutMs = timeoutSec * 1000;
    bool success = m_bridge.StartPersistentProcess(m_ttsServerScriptPath, "", timeoutMs);
    m_serverStarting = false;

    if (success) {
        m_ttsServerReady = true;
        m_ttsFailCount = 0;
        LAppPal::PrintLog("[TTSManager] IndexTTS2 persistent server started successfully");
    } else {
        m_ttsServerReady = false;
        LAppPal::PrintLog("[TTSManager] Failed to start IndexTTS2 server");
    }

    return success;
}

void TTSManager::ShutdownIndexTTS2() {
    if (m_ttsServerReady) {
        LAppPal::PrintLog("[TTSManager] Shutting down IndexTTS2 server...");
        m_bridge.StopPersistentProcess(5000);
        m_ttsServerReady = false;
    }
}

bool TTSManager::InitializeSoVITS(int timeoutSec, bool showGui) {
    LAppPal::PrintLog("[TTSManager] InitializeSoVITS starting...");

    if (m_sovitsServerReady) {
        LAppPal::PrintLog("[TTSManager] SoVITS already initialized");
        return true;
    }

    if (!utils::FileExists(m_sovitsScriptPath)) {
        LAppPal::PrintLog("[TTSManager] sovits_server.py not found: %s", m_sovitsScriptPath.c_str());
        return false;
    }

    // Set progress file environment variable
    if (showGui && !m_progressFilePath.empty()) {
        SetEnvironmentVariableW(L"TTS_PROGRESS_FILE", utils::Utf8ToWide(m_progressFilePath).c_str());
    }

    m_serverStarting = true;
    int timeoutMs = timeoutSec * 1000;
    bool success = m_bridge.StartPersistentProcess(m_sovitsScriptPath, "", timeoutMs);
    m_serverStarting = false;

    if (success) {
        m_sovitsServerReady = true;
        m_sovitsFailCount = 0;
        LAppPal::PrintLog("[TTSManager] SoVITS persistent server started successfully");
    } else {
        m_sovitsServerReady = false;
        LAppPal::PrintLog("[TTSManager] Failed to start SoVITS server");
    }

    return success;
}

void TTSManager::ShutdownSoVITS() {
    if (m_sovitsServerReady) {
        LAppPal::PrintLog("[TTSManager] Shutting down SoVITS server...");
        m_bridge.StopPersistentProcess(5000);
        m_sovitsServerReady = false;
    }
}

bool TTSManager::Speak(const std::string& text, Tone tone, std::string& outputFile) {
    switch (m_engine) {
        case Engine::IndexTTS2:
            return SpeakWithIndexTTS2(text, tone, outputFile);
        case Engine::SoVITS:
            return SpeakWithSoVITS(text, tone, outputFile);
        case Engine::Edge:
        default:
            return SpeakWithEdge(text, tone, outputFile);
    }
}

bool TTSManager::SpeakWithEdge(const std::string& text, Tone tone, std::string& outputFile) {
    if (m_edgeScriptPath.empty()) {
        std::string exeDir = utils::GetCurrentExecutableDir();
        m_edgeScriptPath = exeDir + "\\python\\tts_helper.py";
    }

    std::string tempDir = utils::GetTempDirectory();
    std::string timestamp = utils::GetCurrentTimestamp();
    for (auto& c : timestamp) {
        if (c == ':' || c == '.' || c == ' ') c = '_';
    }
    outputFile = tempDir + "live2d_tts_" + timestamp + ".mp3";

    std::ostringstream args;
    args << "--text \"" << text << "\"";
    args << " --voice \"" << GetVoiceForTone(tone) << "\"";
    args << " --rate \"" << GetRateForTone(tone) << "\"";
    args << " --pitch \"" << GetPitchForTone(tone) << "\"";
    args << " --output \"" << outputFile << "\"";

    LAppPal::PrintLog("[TTSManager] Edge-TTS: %s", m_edgeScriptPath.c_str());

    std::string output;
    if (!m_bridge.RunScript(m_edgeScriptPath, args.str(), output, 60000)) {
        LAppPal::PrintLog("[TTSManager] Edge script failed");
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(output);
        if (j.contains("success") && j["success"].get<bool>()) {
            return true;
        }
    } catch (const std::exception&) {
    }

    return false;
}

bool TTSManager::SendTTSRequest(const std::string& text, const std::string& emotion, const std::string& outputFile) {
    nlohmann::json request;
    request["text"] = text;
    request["output"] = outputFile;
    request["emotion"] = emotion;

    std::string requestStr = request.dump();
    std::string responseStr;

    if (!m_bridge.SendRequest(requestStr, responseStr, 30000)) {
        LAppPal::PrintLog("[TTSManager] SendRequest failed");
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(responseStr);
        if (j.contains("success") && j["success"].get<bool>()) {
            if (j.contains("synthesis_ms")) {
                m_lastSynthTimeMs = j["synthesis_ms"].get<float>();
                LAppPal::PrintLog("[TTSManager] IndexTTS2 synthesis: %.0f ms", m_lastSynthTimeMs);
            }
            // Validate that generated audio has actual content (not just WAV header)
            int64_t fileSize = j.value("file_size", int64_t(0));
            if (fileSize < 1024) {
                LAppPal::PrintLog("[TTSManager] IndexTTS2 generated suspiciously small file: %lld bytes", fileSize);
                return false;
            }
            if (utils::FileExists(outputFile)) {
                std::vector<uint8_t> data = utils::ReadFileToBytes(outputFile);
                if (data.size() < 1024) {
                    LAppPal::PrintLog("[TTSManager] IndexTTS2 output file too small on disk: %zu bytes", data.size());
                    DeleteFileW(utils::Utf8ToWide(outputFile).c_str());
                    return false;
                }
            }
            return true;
        } else {
            std::string err = j.value("error", "unknown");
            LAppPal::PrintLog("[TTSManager] IndexTTS2 server error: %s", err.c_str());
        }
    } catch (const std::exception& e) {
        LAppPal::PrintLog("[TTSManager] IndexTTS2 JSON parse error: %s", e.what());
    }

    return false;
}

bool TTSManager::SendSoVITSRequest(const std::string& text, const std::string& emotion, const std::string& outputFile) {
    nlohmann::json request;
    request["text"] = text;
    request["output"] = outputFile;
    request["emotion"] = emotion;

    std::string requestStr = request.dump();
    std::string responseStr;

    if (!m_bridge.SendRequest(requestStr, responseStr, 30000)) {
        LAppPal::PrintLog("[TTSManager] SoVITS SendRequest failed");
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(responseStr);
        if (j.contains("success") && j["success"].get<bool>()) {
            if (j.contains("synthesis_ms")) {
                m_lastSynthTimeMs = j["synthesis_ms"].get<float>();
                LAppPal::PrintLog("[TTSManager] SoVITS synthesis: %.0f ms", m_lastSynthTimeMs);
            }
            // Validate that generated audio has actual content (not just WAV header)
            int64_t fileSize = j.value("file_size", int64_t(0));
            if (fileSize < 1024) {
                LAppPal::PrintLog("[TTSManager] SoVITS generated suspiciously small file: %lld bytes", fileSize);
                return false;
            }
            // Double-check the file on disk
            if (utils::FileExists(outputFile)) {
                std::vector<uint8_t> data = utils::ReadFileToBytes(outputFile);
                if (data.size() < 1024) {
                    LAppPal::PrintLog("[TTSManager] SoVITS output file too small on disk: %zu bytes", data.size());
                    DeleteFileW(utils::Utf8ToWide(outputFile).c_str());
                    return false;
                }
                // Quick WAV header check: data size at offset 40
                if (data.size() >= 44) {
                    int32_t wavDataSize = *reinterpret_cast<const int32_t*>(&data[40]);
                    LAppPal::PrintLog("[TTSManager] SoVITS WAV: total=%zu data=%d bytes", data.size(), wavDataSize);
                    if (wavDataSize == 0) {
                        LAppPal::PrintLog("[TTSManager] SoVITS output has zero audio data — likely silent");
                        DeleteFileW(utils::Utf8ToWide(outputFile).c_str());
                        return false;
                    }
                }
            }
            return true;
        } else {
            std::string err = j.value("error", "unknown");
            LAppPal::PrintLog("[TTSManager] SoVITS server error: %s", err.c_str());
        }
    } catch (const std::exception& e) {
        LAppPal::PrintLog("[TTSManager] SoVITS JSON parse error: %s", e.what());
    }

    return false;
}

bool TTSManager::SpeakWithSoVITS(const std::string& text, Tone tone, std::string& outputFile) {
    if (!m_sovitsServerReady) {
        LAppPal::PrintLog("[TTSManager] SoVITS server not ready, waiting...");
        if (!WaitForServerReady(300)) {
            LAppPal::PrintLog("[TTSManager] SoVITS server failed to become ready");
            return false;
        }
    }

    std::string tempDir = utils::GetTempDirectory();
    std::string timestamp = utils::GetCurrentTimestamp();
    for (auto& c : timestamp) {
        if (c == ':' || c == '.' || c == ' ') c = '_';
    }
    outputFile = tempDir + "live2d_tts_" + timestamp + ".wav";

    std::string emotion = GetEmotionForTone(tone);

    bool success = SendSoVITSRequest(text, emotion, outputFile);

    if (success) {
        m_sovitsFailCount = 0;
        return true;
    }

    m_sovitsFailCount++;
    LAppPal::PrintLog("[TTSManager] SoVITS request failed (fail count: %d/%d)",
                      m_sovitsFailCount, MAX_SOVITS_FAIL_COUNT);

    if (m_sovitsFailCount >= MAX_SOVITS_FAIL_COUNT) {
        LAppPal::PrintLog("[TTSManager] Too many SoVITS failures, giving up");
        m_sovitsFailCount = 0;
        ShutdownSoVITS();
        return false;
    }

    if (!m_bridge.IsPersistentAlive()) {
        LAppPal::PrintLog("[TTSManager] SoVITS server died, attempting restart...");
        if (m_bridge.RestartPersistentProcess()) {
            m_sovitsServerReady = true;
            LAppPal::PrintLog("[TTSManager] Server restarted, retrying request");
            return SendSoVITSRequest(text, emotion, outputFile);
        } else {
            LAppPal::PrintLog("[TTSManager] Restart failed");
            m_sovitsFailCount = 0;
            m_sovitsServerReady = false;
            return false;
        }
    }

    return false;
}

bool TTSManager::SpeakWithIndexTTS2(const std::string& text, Tone tone, std::string& outputFile) {
    if (!m_ttsServerReady) {
        LAppPal::PrintLog("[TTSManager] IndexTTS2 server not ready, waiting...");
        if (!WaitForServerReady(90)) {
            LAppPal::PrintLog("[TTSManager] IndexTTS2 server failed to become ready");
            return false;
        }
    }

    std::string tempDir = utils::GetTempDirectory();
    std::string timestamp = utils::GetCurrentTimestamp();
    for (auto& c : timestamp) {
        if (c == ':' || c == '.' || c == ' ') c = '_';
    }
    outputFile = tempDir + "live2d_tts_" + timestamp + ".wav";

    std::string emotion = GetEmotionForTone(tone);

    bool success = SendTTSRequest(text, emotion, outputFile);

    if (success) {
        m_ttsFailCount = 0;
        return true;
    }

    m_ttsFailCount++;
    LAppPal::PrintLog("[TTSManager] IndexTTS2 request failed (fail count: %d/%d)",
                      m_ttsFailCount, MAX_TTS_FAIL_COUNT);

    if (m_ttsFailCount >= MAX_TTS_FAIL_COUNT) {
        LAppPal::PrintLog("[TTSManager] Too many IndexTTS2 failures, giving up");
        m_ttsFailCount = 0;
        ShutdownIndexTTS2();
        return false;
    }

    if (!m_bridge.IsPersistentAlive()) {
        LAppPal::PrintLog("[TTSManager] IndexTTS2 server died, attempting restart...");
        if (m_bridge.RestartPersistentProcess()) {
            m_ttsServerReady = true;
            LAppPal::PrintLog("[TTSManager] Server restarted, retrying request");
            return SendTTSRequest(text, emotion, outputFile);
        } else {
            LAppPal::PrintLog("[TTSManager] Restart failed");
            m_ttsFailCount = 0;
            m_ttsServerReady = false;
            return false;
        }
    }

    return false;
}

std::string TTSManager::GetVoiceForTone(Tone tone) const {
    switch (tone) {
        case Tone::Happy:     return m_voiceHappy;
        case Tone::Surprised: return m_voiceSurprised;
        case Tone::Normal:
        default:              return m_voiceNormal;
    }
}

std::string TTSManager::GetRateForTone(Tone tone) const {
    switch (tone) {
        case Tone::Happy:     return m_rateHappy;
        case Tone::Surprised: return m_rateSurprised;
        case Tone::Normal:
        default:              return m_rate;
    }
}

std::string TTSManager::GetPitchForTone(Tone tone) const {
    switch (tone) {
        case Tone::Happy:     return m_pitchHappy;
        case Tone::Surprised: return m_pitchSurprised;
        case Tone::Normal:
        default:              return m_pitch;
    }
}

std::string TTSManager::GetEmotionForTone(Tone tone) const {
    switch (tone) {
        case Tone::Happy:     return "happy";
        case Tone::Surprised: return "surprised";
        case Tone::Normal:
        default:              return "neutral";
    }
}
