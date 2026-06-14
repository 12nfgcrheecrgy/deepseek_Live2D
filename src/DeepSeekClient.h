#pragma once

#include <string>
#include "PythonBridge.h"

class DeepSeekClient {
public:
    DeepSeekClient() = default;
    ~DeepSeekClient() = default;

    DeepSeekClient(const DeepSeekClient&) = delete;
    DeepSeekClient& operator=(const DeepSeekClient&) = delete;

    void SetConfig(const std::string& apiKey, const std::string& apiBase, const std::string& model,
                   int maxTokens, float temperature, const std::string& systemPrompt);
    bool Chat(const std::string& userMessage, const std::string& screenContent, std::string& outReply);

private:
    std::string m_apiKey;
    std::string m_apiBase = "https://api.deepseek.com/v1";
    std::string m_model = "deepseek-chat";
    int m_maxTokens = 1024;
    float m_temperature = 0.8f;
    std::string m_systemPrompt;

    PythonBridge m_bridge;
    std::string m_scriptPath;
};