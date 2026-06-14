#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include "PythonBridge.h"

class OCRManager {
public:
    OCRManager() = default;
    ~OCRManager() = default;

    OCRManager(const OCRManager&) = delete;
    OCRManager& operator=(const OCRManager&) = delete;

    void SetConfig(const std::string& language, int intervalSeconds, bool enabled);

    bool CaptureAndOCR();
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    std::string GetLastText() const;

private:
    std::string m_language = "chi_sim+eng";
    int m_intervalSeconds = 5;
    std::atomic<bool> m_enabled = false;
    std::string m_lastText;
    mutable std::mutex m_mutex;

    PythonBridge m_bridge;
    std::string m_scriptPath;
};