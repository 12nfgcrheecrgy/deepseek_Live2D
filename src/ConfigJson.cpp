#include "Config.h"
#include "utils.h"
#include <fstream>
#include <sstream>

static std::string GetStringValue(const std::string& json, const std::string& key, size_t& pos) {
    std::string searchKey = "\"" + key + "\"";
    size_t kp = json.find(searchKey, pos);
    if (kp == std::string::npos) return "";
    kp += searchKey.length();
    kp = json.find(':', kp);
    if (kp == std::string::npos) return "";
    kp++;
    while (kp < json.length() && (json[kp] == ' ' || json[kp] == '\t' || json[kp] == '\n' || json[kp] == '\r'))
        kp++;
    if (kp >= json.length() || json[kp] != '"') return "";
    kp++;
    std::string result;
    while (kp < json.length() && json[kp] != '"') {
        if (json[kp] == '\\' && kp + 1 < json.length()) {
            kp++;
        }
        result += json[kp];
        kp++;
    }
    pos = kp;
    return result;
}

static int GetIntValue(const std::string& json, const std::string& key, size_t& pos) {
    std::string searchKey = "\"" + key + "\"";
    size_t kp = json.find(searchKey, pos);
    if (kp == std::string::npos) return 0;
    kp += searchKey.length();
    kp = json.find(':', kp);
    if (kp == std::string::npos) return 0;
    kp++;
    while (kp < json.length() && (json[kp] == ' ' || json[kp] == '\t' || json[kp] == '\n' || json[kp] == '\r'))
        kp++;
    std::string num;
    while (kp < json.length() && (isdigit(json[kp]) || json[kp] == '-')) {
        num += json[kp];
        kp++;
    }
    pos = kp;
    return num.empty() ? 0 : std::stoi(num);
}

static float GetFloatValue(const std::string& json, const std::string& key, size_t& pos) {
    std::string searchKey = "\"" + key + "\"";
    size_t kp = json.find(searchKey, pos);
    if (kp == std::string::npos) return 0.0f;
    kp += searchKey.length();
    kp = json.find(':', kp);
    if (kp == std::string::npos) return 0.0f;
    kp++;
    while (kp < json.length() && (json[kp] == ' ' || json[kp] == '\t' || json[kp] == '\n' || json[kp] == '\r'))
        kp++;
    std::string num;
    while (kp < json.length() && (isdigit(json[kp]) || json[kp] == '-' || json[kp] == '.')) {
        num += json[kp];
        kp++;
    }
    pos = kp;
    return num.empty() ? 0.0f : static_cast<float>(std::stod(num));
}

static bool GetBoolValue(const std::string& json, const std::string& key, size_t& pos) {
    std::string searchKey = "\"" + key + "\"";
    size_t kp = json.find(searchKey, pos);
    if (kp == std::string::npos) return false;
    kp += searchKey.length();
    kp = json.find(':', kp);
    if (kp == std::string::npos) return false;
    kp++;
    while (kp < json.length() && (json[kp] == ' ' || json[kp] == '\t' || json[kp] == '\n' || json[kp] == '\r'))
        kp++;
    if (kp + 4 <= json.length() && json.substr(kp, 4) == "true") {
        pos = kp + 4;
        return true;
    }
    if (kp + 5 <= json.length() && json.substr(kp, 5) == "false") {
        pos = kp + 5;
        return false;
    }
    return false;
}

static size_t FindObjectStart(const std::string& json, const std::string& key, size_t from = 0) {
    std::string searchKey = "\"" + key + "\"";
    size_t kp = json.find(searchKey, from);
    if (kp == std::string::npos) return std::string::npos;
    kp += searchKey.length();
    kp = json.find(':', kp);
    if (kp == std::string::npos) return std::string::npos;
    kp = json.find('{', kp);
    return kp;
}

bool Config::Load(const std::string& filepath) {
    std::string json = utils::ReadFileToString(filepath);
    if (json.empty()) return false;

    Config& cfg = GetInstance();
    size_t pos = 0;

    size_t dsPos = FindObjectStart(json, "deepseek");
    if (dsPos != std::string::npos) {
        cfg.m_deepseek.model = GetStringValue(json, "model", dsPos);
        cfg.m_deepseek.api_key = GetStringValue(json, "api_key", dsPos);
        cfg.m_deepseek.api_base = GetStringValue(json, "api_base", dsPos);
        cfg.m_deepseek.max_tokens = GetIntValue(json, "max_tokens", dsPos);
        cfg.m_deepseek.temperature = GetFloatValue(json, "temperature", dsPos);
        cfg.m_deepseek.system_prompt = GetStringValue(json, "system_prompt", dsPos);
    }

    size_t ttsPos = FindObjectStart(json, "tts");
    if (ttsPos != std::string::npos) {
        cfg.m_tts.engine = GetStringValue(json, "engine", ttsPos);
        cfg.m_tts.voice = GetStringValue(json, "voice", ttsPos);
        cfg.m_tts.rate = GetStringValue(json, "rate", ttsPos);
        cfg.m_tts.pitch = GetStringValue(json, "pitch", ttsPos);
        cfg.m_tts.voice_happy = GetStringValue(json, "voice_happy", ttsPos);
        cfg.m_tts.voice_surprised = GetStringValue(json, "voice_surprised", ttsPos);
        cfg.m_tts.voice_normal = GetStringValue(json, "voice_normal", ttsPos);
        cfg.m_tts.rate_happy = GetStringValue(json, "rate_happy", ttsPos);
        cfg.m_tts.rate_surprised = GetStringValue(json, "rate_surprised", ttsPos);
        cfg.m_tts.pitch_happy = GetStringValue(json, "pitch_happy", ttsPos);
        cfg.m_tts.pitch_surprised = GetStringValue(json, "pitch_surprised", ttsPos);
        cfg.m_tts.indextts2_model_dir = GetStringValue(json, "indextts2_model_dir", ttsPos);
        cfg.m_tts.indextts2_config = GetStringValue(json, "indextts2_config", ttsPos);
        cfg.m_tts.indextts2_reference_audio = GetStringValue(json, "indextts2_reference_audio", ttsPos);
        cfg.m_tts.indextts2_use_fp16 = GetBoolValue(json, "indextts2_use_fp16", ttsPos);
        cfg.m_tts.indextts2_use_openvino = GetBoolValue(json, "indextts2_use_openvino", ttsPos);
        cfg.m_tts.indextts2_openvino_device = GetStringValue(json, "indextts2_openvino_device", ttsPos);
        cfg.m_tts.sovits_model_path = GetStringValue(json, "sovits_model_path", ttsPos);
        cfg.m_tts.sovits_config_path = GetStringValue(json, "sovits_config_path", ttsPos);
        cfg.m_tts.sovits_diffusion_path = GetStringValue(json, "sovits_diffusion_path", ttsPos);
        cfg.m_tts.sovits_diffusion_config = GetStringValue(json, "sovits_diffusion_config", ttsPos);
        cfg.m_tts.sovits_reference_audio = GetStringValue(json, "sovits_reference_audio", ttsPos);
        cfg.m_tts.sovits_reference_dir = GetStringValue(json, "sovits_reference_dir", ttsPos);
        cfg.m_tts.sovits_gpt_weights = GetStringValue(json, "sovits_gpt_weights", ttsPos);
        cfg.m_tts.sovits_fp16 = GetBoolValue(json, "sovits_fp16", ttsPos);
    }

    size_t ocrPos = FindObjectStart(json, "ocr");
    if (ocrPos != std::string::npos) {
        cfg.m_ocr.language = GetStringValue(json, "language", ocrPos);
        cfg.m_ocr.interval_seconds = GetIntValue(json, "interval_seconds", ocrPos);
        cfg.m_ocr.enabled = GetBoolValue(json, "enabled", ocrPos);
    }

    size_t winPos = FindObjectStart(json, "window");
    if (winPos != std::string::npos) {
        pos = winPos;
        cfg.m_window.width = GetIntValue(json, "width", pos);
        cfg.m_window.height = GetIntValue(json, "height", pos);
        cfg.m_window.margin_right = GetIntValue(json, "margin_right", pos);
        cfg.m_window.margin_bottom = GetIntValue(json, "margin_bottom", pos);
        cfg.m_window.scale = GetFloatValue(json, "scale", pos);
        cfg.m_window.always_on_top = GetBoolValue(json, "always_on_top", pos);
    }

    size_t mdlPos = FindObjectStart(json, "model");
    if (mdlPos != std::string::npos) {
        pos = mdlPos;
        cfg.m_model.name = GetStringValue(json, "name", pos);
        cfg.m_model.directory = GetStringValue(json, "directory", pos);
    }

    return true;
}

bool Config::Save(const std::string& filepath) const {
    std::ofstream file(filepath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "    \"deepseek\": {\n";
    file << "        \"model\": \"" << m_deepseek.model << "\",\n";
    file << "        \"api_key\": \"" << m_deepseek.api_key << "\",\n";
    file << "        \"api_base\": \"" << m_deepseek.api_base << "\",\n";
    file << "        \"max_tokens\": " << m_deepseek.max_tokens << ",\n";
    file << "        \"temperature\": " << m_deepseek.temperature << ",\n";
    file << "        \"system_prompt\": \"" << m_deepseek.system_prompt << "\"\n";
    file << "    },\n";
    file << "    \"tts\": {\n";
    file << "        \"engine\": \"" << m_tts.engine << "\",\n";
    file << "        \"voice\": \"" << m_tts.voice << "\",\n";
    file << "        \"rate\": \"" << m_tts.rate << "\",\n";
    file << "        \"pitch\": \"" << m_tts.pitch << "\",\n";
    file << "        \"voice_happy\": \"" << m_tts.voice_happy << "\",\n";
    file << "        \"voice_surprised\": \"" << m_tts.voice_surprised << "\",\n";
    file << "        \"voice_normal\": \"" << m_tts.voice_normal << "\",\n";
    file << "        \"rate_happy\": \"" << m_tts.rate_happy << "\",\n";
    file << "        \"rate_surprised\": \"" << m_tts.rate_surprised << "\",\n";
    file << "        \"pitch_happy\": \"" << m_tts.pitch_happy << "\",\n";
    file << "        \"pitch_surprised\": \"" << m_tts.pitch_surprised << "\",\n";
    file << "        \"indextts2_model_dir\": \"" << m_tts.indextts2_model_dir << "\",\n";
    file << "        \"indextts2_config\": \"" << m_tts.indextts2_config << "\",\n";
    file << "        \"indextts2_reference_audio\": \"" << m_tts.indextts2_reference_audio << "\",\n";
    file << "        \"indextts2_use_fp16\": " << (m_tts.indextts2_use_fp16 ? "true" : "false") << ",\n";
    file << "        \"indextts2_use_openvino\": " << (m_tts.indextts2_use_openvino ? "true" : "false") << ",\n";
    file << "        \"indextts2_openvino_device\": \"" << m_tts.indextts2_openvino_device << "\",\n";
    file << "        \"sovits_model_path\": \"" << m_tts.sovits_model_path << "\",\n";
    file << "        \"sovits_config_path\": \"" << m_tts.sovits_config_path << "\",\n";
    file << "        \"sovits_diffusion_path\": \"" << m_tts.sovits_diffusion_path << "\",\n";
    file << "        \"sovits_diffusion_config\": \"" << m_tts.sovits_diffusion_config << "\",\n";
    file << "        \"sovits_reference_audio\": \"" << m_tts.sovits_reference_audio << "\",\n";
    file << "        \"sovits_reference_dir\": \"" << m_tts.sovits_reference_dir << "\",\n";
    file << "        \"sovits_gpt_weights\": \"" << m_tts.sovits_gpt_weights << "\",\n";
    file << "        \"sovits_fp16\": " << (m_tts.sovits_fp16 ? "true" : "false") << "\n";
    file << "    },\n";
    file << "    \"ocr\": {\n";
    file << "        \"language\": \"" << m_ocr.language << "\",\n";
    file << "        \"interval_seconds\": " << m_ocr.interval_seconds << ",\n";
    file << "        \"enabled\": " << (m_ocr.enabled ? "true" : "false") << "\n";
    file << "    },\n";
    file << "    \"window\": {\n";
    file << "        \"width\": " << m_window.width << ",\n";
    file << "        \"height\": " << m_window.height << ",\n";
    file << "        \"margin_right\": " << m_window.margin_right << ",\n";
    file << "        \"margin_bottom\": " << m_window.margin_bottom << ",\n";
    file << "        \"scale\": " << m_window.scale << ",\n";
    file << "        \"always_on_top\": " << (m_window.always_on_top ? "true" : "false") << "\n";
    file << "    },\n";
    file << "    \"model\": {\n";
    file << "        \"name\": \"" << m_model.name << "\",\n";
    file << "        \"directory\": \"" << m_model.directory << "\"\n";
    file << "    }\n";
    file << "}\n";

    return true;
}