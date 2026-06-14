#pragma once

#include <string>
#include "PythonBridge.h"

class TTSManager {
public:
    enum class Tone {
        Normal,
        Happy,
        Surprised
    };

    enum class Engine {
        Edge,
        IndexTTS2,
        SoVITS
    };

    TTSManager() = default;
    ~TTSManager();

    TTSManager(const TTSManager&) = delete;
    TTSManager& operator=(const TTSManager&) = delete;

    void SetConfig(const std::string& voice, const std::string& rate, const std::string& pitch,
                   const std::string& voiceHappy, const std::string& voiceSurprised, const std::string& voiceNormal,
                   const std::string& rateHappy, const std::string& rateSurprised,
                   const std::string& pitchHappy, const std::string& pitchSurprised);

    void SetEngine(Engine engine) { m_engine = engine; }
    Engine GetEngine() const { return m_engine; }

    void SetIndexTTS2Config(const std::string& modelDir, const std::string& configPath,
                            const std::string& referenceAudio, bool useFp16, bool useOpenVino,
                            const std::string& openvinoDevice);

    void SetSoVITSConfig(const std::string& modelPath, const std::string& configPath,
                         const std::string& diffusionPath, const std::string& diffusionConfig,
                         const std::string& referenceAudio, bool useFp16,
                         const std::string& gptWeights);

    bool InitializeIndexTTS2(int timeoutSec = 60, bool showGui = true);
    void ShutdownIndexTTS2();

    bool InitializeSoVITS(int timeoutSec = 300, bool showGui = true);
    void ShutdownSoVITS();

    bool WaitForServerReady(int timeoutSec);
    bool StartLoaderGUI(const std::string& engineName);
    void TerminateLoaderGUI();

    bool Speak(const std::string& text, Tone tone, std::string& outputFile);

    float GetLastSynthTimeMs() const { return m_lastSynthTimeMs; }
    bool IsIndexTTS2Ready() const { return m_ttsServerReady; }
    bool IsSoVITSReady() const { return m_sovitsServerReady; }

private:
    bool SpeakWithEdge(const std::string& text, Tone tone, std::string& outputFile);
    bool SpeakWithIndexTTS2(const std::string& text, Tone tone, std::string& outputFile);
    bool SpeakWithSoVITS(const std::string& text, Tone tone, std::string& outputFile);
    bool SendTTSRequest(const std::string& text, const std::string& emotion, const std::string& outputFile);
    bool SendSoVITSRequest(const std::string& text, const std::string& emotion, const std::string& outputFile);

    std::string GetVoiceForTone(Tone tone) const;
    std::string GetRateForTone(Tone tone) const;
    std::string GetPitchForTone(Tone tone) const;
    std::string GetEmotionForTone(Tone tone) const;

    std::string m_voice = "zh-CN-XiaoxiaoNeural";
    std::string m_rate = "+0%";
    std::string m_pitch = "+0Hz";
    std::string m_voiceHappy = "zh-CN-XiaoxiaoNeural";
    std::string m_voiceSurprised = "zh-CN-XiaoyiNeural";
    std::string m_voiceNormal = "zh-CN-XiaoxiaoNeural";
    std::string m_rateHappy = "+10%";
    std::string m_rateSurprised = "+15%";
    std::string m_pitchHappy = "+20Hz";
    std::string m_pitchSurprised = "+30Hz";

    Engine m_engine = Engine::Edge;

    std::string m_indexTts2ModelDir;
    std::string m_indexTts2ConfigPath;
    std::string m_indexTts2ReferenceAudio;
    bool m_indexTts2UseFp16 = true;
    bool m_indexTts2UseOpenVino = false;
    std::string m_indexTts2OpenvinoDevice = "CPU";

    PythonBridge m_bridge;
    std::string m_edgeScriptPath;
    std::string m_indexTts2ScriptPath;

    std::string m_ttsServerScriptPath;
    bool m_ttsServerReady = false;
    int m_ttsFailCount = 0;
    static const int MAX_TTS_FAIL_COUNT = 3;

    std::string m_sovitsModelPath;
    std::string m_sovitsConfigPath;
    std::string m_sovitsDiffusionPath;
    std::string m_sovitsDiffusionConfig;
    std::string m_sovitsReferenceAudio;
    std::string m_sovitsGptWeights;
    bool m_sovitsUseFp16 = true;
    std::string m_sovitsScriptPath;
    bool m_sovitsServerReady = false;
    int m_sovitsFailCount = 0;
    static const int MAX_SOVITS_FAIL_COUNT = 3;

    float m_lastSynthTimeMs = 0.0f;

    // Loader GUI
    std::string m_loaderGuiPath;
    std::string m_progressFilePath;
    HANDLE m_hLoaderGuiProcess = nullptr;
    bool m_serverStarting = false;
};