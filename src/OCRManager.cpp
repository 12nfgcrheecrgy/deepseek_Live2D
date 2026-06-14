#include "OCRManager.h"
#include "utils.h"
#include "nlohmann/json.hpp"
#include <sstream>

void OCRManager::SetConfig(const std::string& language, int intervalSeconds, bool enabled) {
    m_language = language;
    m_intervalSeconds = intervalSeconds;
    m_enabled.store(enabled);

    std::string exeDir = utils::GetCurrentExecutableDir();
    m_scriptPath = exeDir + "\\python\\ocr_helper.py";
}

bool OCRManager::CaptureAndOCR() {
    if (!m_enabled.load()) {
        return false;
    }

    if (m_scriptPath.empty()) {
        std::string exeDir = utils::GetCurrentExecutableDir();
        m_scriptPath = exeDir + "\\python\\ocr_helper.py";
    }

    std::ostringstream args;
    args << "--language \"" << m_language << "\"";
    args << " --mode full";

    std::string output;
    if (!m_bridge.RunScript(m_scriptPath, args.str(), output, 30000)) {
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(output);
        if (j.contains("success") && j["success"].get<bool>()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastText = j["text"].get<std::string>();
            return true;
        }
    } catch (const std::exception&) {
    }

    return false;
}

void OCRManager::SetEnabled(bool enabled) {
    m_enabled.store(enabled);
}

bool OCRManager::IsEnabled() const {
    return m_enabled.load();
}

std::string OCRManager::GetLastText() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastText;
}