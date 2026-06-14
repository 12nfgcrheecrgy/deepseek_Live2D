#include "LAppPal.h"
#include "utils.h"
#include <cstdio>
#include <cstdarg>
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <mutex>

double LAppPal::s_currentFrame = 0.0;
double LAppPal::s_lastFrame = 0.0;
double LAppPal::s_deltaTime = 0.0;
LogLevel LAppPal::s_logLevel = LogLevel::Debug;
static std::mutex g_logMutex;

static const char* LevelPrefix(LogLevel level) {
    switch (level) {
        case LogLevel::Error: return "ERROR";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        default: return "LOG";
    }
}

void LAppPal::WriteLog(LogLevel level, const char* text) {
    if (level > s_logLevel) return;

    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream file("Live2D_debug.log", std::ios::app);
    if (file.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_now;
        localtime_s(&tm_now, &t);
        char timeBuf[32];
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm_now);
        file << "[" << timeBuf << "] [" << LevelPrefix(level) << "] " << text << std::endl;
    }
}

void LAppPal::LogError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    OutputDebugStringA("[Live2D:ERROR] ");
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
    WriteLog(LogLevel::Error, buffer);
}

void LAppPal::LogDebug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    OutputDebugStringA("[Live2D:DEBUG] ");
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
    WriteLog(LogLevel::Debug, buffer);
}

void LAppPal::LogInfo(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    OutputDebugStringA("[Live2D:INFO] ");
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
    WriteLog(LogLevel::Info, buffer);
}

void LAppPal::PrintLog(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    OutputDebugStringA("[Live2D] ");
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");

    WriteLog(LogLevel::Info, buffer);
}

void LAppPal::PrintMessage(const char* message) {
    OutputDebugStringA("[Live2D] ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");

    WriteLog(LogLevel::Info, message);
}

void LAppPal::UpdateTime() {
    s_lastFrame = s_currentFrame;

    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    s_currentFrame = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000000.0;

    if (s_lastFrame == 0.0) {
        s_lastFrame = s_currentFrame;
        s_deltaTime = 1.0 / 60.0;
    } else {
        s_deltaTime = s_currentFrame - s_lastFrame;
    }
}

double LAppPal::GetDeltaTime() {
    return s_deltaTime;
}


std::vector<uint8_t> LAppPal::LoadFileAsBytes(const std::string& filePath) {
    return utils::ReadFileToBytes(filePath);
}

Csm::csmByte* LAppPal::LoadFileBytes(const std::string filePath, Csm::csmSizeInt* outSize) {
    std::wstring wPath = utils::Utf8ToWide(filePath);
    HANDLE hFile = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        // Try loading relative to the executable directory
        std::string exeDir = utils::GetCurrentExecutableDir();
        std::string altPath = exeDir + "\\" + filePath;
        wPath = utils::Utf8ToWide(altPath);
        hFile = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            LogError("LoadFileBytes: failed to open %s (also tried %s)", filePath.c_str(), altPath.c_str());
            *outSize = 0;
            return nullptr;
        }
    }
    DWORD fileSize = GetFileSize(hFile, nullptr);
    Csm::csmByte* buffer = new Csm::csmByte[fileSize];
    DWORD bytesRead = 0;
    ReadFile(hFile, buffer, fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);
    *outSize = static_cast<Csm::csmSizeInt>(bytesRead);
    return buffer;
}

void LAppPal::ReleaseFileBytes(Csm::csmByte* byteData) {
    delete[] byteData;
}
