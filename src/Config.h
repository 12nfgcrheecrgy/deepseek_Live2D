#pragma once

#include <string>
#include <memory>
#include <mutex>

// Forward declare
namespace nlohmann {
    class json;
}

struct DeepSeekConfig {
    std::string model = "deepseek-chat";
    std::string api_key;
    std::string api_base = "https://api.deepseek.com/v1";
    int max_tokens = 1024;
    float temperature = 0.8f;
    std::string system_prompt;
};

struct TTSConfig {
    std::string engine = "edge";
    std::string voice = "zh-CN-XiaoxiaoNeural";
    std::string rate = "+0%";
    std::string pitch = "+0Hz";
    std::string voice_happy = "zh-CN-XiaoxiaoNeural";
    std::string voice_surprised = "zh-CN-XiaoyiNeural";
    std::string voice_normal = "zh-CN-XiaoxiaoNeural";
    std::string rate_happy = "+10%";
    std::string rate_surprised = "+15%";
    std::string pitch_happy = "+20Hz";
    std::string pitch_surprised = "+30Hz";
    std::string indextts2_model_dir;
    std::string indextts2_config;
    std::string indextts2_reference_audio = "music/reference_voice.wav";
    bool indextts2_use_fp16 = true;
    bool indextts2_use_openvino = false;
    std::string indextts2_openvino_device = "CPU";
    std::string sovits_model_path;
    std::string sovits_config_path;
    std::string sovits_diffusion_path;
    std::string sovits_diffusion_config;
    std::string sovits_reference_audio = "music/reference_voice.wav";
    std::string sovits_reference_dir;
    std::string sovits_gpt_weights;
    bool sovits_fp16 = true;
};

struct OCRConfig {
    std::string language = "chi_sim+eng";
    int interval_seconds = 5;
    bool enabled = true;
};

struct WindowConfig {
    int width = 400;
    int height = 500;
    int margin_right = 20;
    int margin_bottom = 20;
    float scale = 1.0f;
    bool always_on_top = true;
};

struct ModelConfig {
    std::string name = "Furina";
    std::string directory = "Resources/model/Furina";
};

class Config {
public:
    static bool Load(const std::string& filepath);
    static Config& GetInstance();

    const DeepSeekConfig& GetDeepSeek() const { return m_deepseek; }
    const TTSConfig& GetTTS() const { return m_tts; }
    const OCRConfig& GetOCR() const { return m_ocr; }
    const WindowConfig& GetWindow() const { return m_window; }
    const ModelConfig& GetModel() const { return m_model; }

    DeepSeekConfig& GetDeepSeekMutable() { return m_deepseek; }
    TTSConfig& GetTTSMutable() { return m_tts; }
    OCRConfig& GetOCRMutable() { return m_ocr; }
    WindowConfig& GetWindowMutable() { return m_window; }
    ModelConfig& GetModelMutable() { return m_model; }

    bool Save(const std::string& filepath) const;

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    DeepSeekConfig m_deepseek;
    TTSConfig m_tts;
    OCRConfig m_ocr;
    WindowConfig m_window;
    ModelConfig m_model;
};