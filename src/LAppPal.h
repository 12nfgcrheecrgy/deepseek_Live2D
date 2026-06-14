#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "CubismFramework.hpp"

namespace Csm = Live2D::Cubism::Framework;

enum class LogLevel {
    Error = 0,
    Info = 1,
    Debug = 2
};

class LAppPal {
public:
    static void SetLogLevel(LogLevel level) { s_logLevel = level; }
    static LogLevel GetLogLevel() { return s_logLevel; }

    static void LogError(const char* format, ...);
    static void LogDebug(const char* format, ...);
    static void LogInfo(const char* format, ...);
    static void PrintLog(const char* format, ...);  // kept for Cubism SDK callback
    static void PrintMessage(const char* message);
    static void UpdateTime();
    static double GetDeltaTime();
    static std::vector<uint8_t> LoadFileAsBytes(const std::string& filePath);

    static Csm::csmByte* LoadFileBytes(const std::string filePath, Csm::csmSizeInt* outSize);
    static void ReleaseFileBytes(Csm::csmByte* byteData);

private:
    static void WriteLog(LogLevel level, const char* text);
    static double s_currentFrame;
    static double s_lastFrame;
    static double s_deltaTime;
    static LogLevel s_logLevel;
};
