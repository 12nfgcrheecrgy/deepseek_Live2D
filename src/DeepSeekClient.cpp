#include "DeepSeekClient.h"
#include "utils.h"
#include "LAppPal.h"
#include "nlohmann/json.hpp"
#include <sstream>
#include <fstream>

void DeepSeekClient::SetConfig(const std::string& apiKey, const std::string& apiBase, const std::string& model,
                                int maxTokens, float temperature, const std::string& systemPrompt) {
    m_apiKey = apiKey;
    m_apiBase = apiBase;
    m_model = model;
    m_maxTokens = maxTokens;
    m_temperature = temperature;
    m_systemPrompt = systemPrompt;

    std::string exeDir = utils::GetCurrentExecutableDir();
    m_scriptPath = exeDir + "\\python\\deepseek_api.py";
    LAppPal::PrintLog("[DeepSeek] SetConfig: script=%s", m_scriptPath.c_str());
}

bool DeepSeekClient::Chat(const std::string& userMessage, const std::string& screenContent, std::string& outReply) {
    LAppPal::PrintLog("[DeepSeek] Chat called, userMessage length=%zu, screenContent length=%zu",
                      userMessage.size(), screenContent.size());

    if (m_scriptPath.empty()) {
        std::string exeDir = utils::GetCurrentExecutableDir();
        m_scriptPath = exeDir + "\\python\\deepseek_api.py";
    }

    if (!utils::FileExists(m_scriptPath)) {
        LAppPal::PrintLog("[DeepSeek] ERROR: Script not found: %s", m_scriptPath.c_str());
        return false;
    }

    // Write messages to a temp JSON file to avoid command-line escaping issues
    std::string tempDir = utils::GetTempDirectory();
    std::string timestamp = utils::GetCurrentTimestamp();
    for (auto& c : timestamp) { if (c == ':' || c == '.' || c == ' ') c = '_'; }
    std::string inputFile = tempDir + "deepseek_input_" + timestamp + ".json";

    {
        nlohmann::json input;
        input["user_message"] = userMessage;
        if (!screenContent.empty()) {
            input["screen_content"] = screenContent;
        }
        std::ofstream ofs(inputFile, std::ios::binary);
        if (!ofs.is_open()) {
            LAppPal::PrintLog("[DeepSeek] ERROR: Cannot write input file: %s", inputFile.c_str());
            return false;
        }
        std::string jsonStr = input.dump();
        ofs.write(jsonStr.c_str(), jsonStr.size());
        ofs.close();
    }

    std::string pythonExe = m_bridge.GetPythonExe();
    LAppPal::PrintLog("[DeepSeek] Python exe: %s", pythonExe.c_str());

    std::ostringstream args;
    args << "--api-key \"" << m_apiKey << "\"";
    args << " --api-base \"" << m_apiBase << "\"";
    args << " --model \"" << m_model << "\"";
    args << " --max-tokens " << m_maxTokens;
    args << " --temperature " << m_temperature;
    args << " --system-prompt \"" << m_systemPrompt << "\"";
    args << " --input-file \"" << inputFile << "\"";

    LAppPal::PrintLog("[DeepSeek] Running script with timeout 60000ms");

    std::string output;
    if (!m_bridge.RunScript(m_scriptPath, args.str(), output, 60000)) {
        LAppPal::PrintLog("[DeepSeek] RunScript returned false, output length=%zu", output.size());
        if (!output.empty()) {
            LAppPal::PrintLog("[DeepSeek] Output: %s", output.c_str());
        }
        // Clean up temp file
        DeleteFileW(utils::Utf8ToWide(inputFile).c_str());
        return false;
    }

    LAppPal::PrintLog("[DeepSeek] RunScript returned output length=%zu", output.size());

    // Clean up temp file
    DeleteFileW(utils::Utf8ToWide(inputFile).c_str());

    try {
        nlohmann::json j = nlohmann::json::parse(output);
        if (j.contains("success") && j["success"].get<bool>()) {
            outReply = j["reply"].get<std::string>();
            LAppPal::PrintLog("[DeepSeek] Success, reply length=%zu", outReply.size());
            return true;
        } else {
            std::string err = j.value("error", "unknown");
            LAppPal::PrintLog("[DeepSeek] API error: %s", err.c_str());
        }
    } catch (const std::exception& e) {
        LAppPal::PrintLog("[DeepSeek] JSON parse error: %s", e.what());
    }

    return false;
}
