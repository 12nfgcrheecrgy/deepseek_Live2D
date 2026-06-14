#pragma once

#include <string>
#include <vector>
#include <random>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <Windows.h>
#include <ShlObj.h>

namespace utils {

inline std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &result[0], len);
    return result;
}

inline std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), &result[0], len, nullptr, nullptr);
    return result;
}

inline std::string ReadFileToString(const std::string& filepath) {
    std::wstring wpath = Utf8ToWide(filepath);
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return "";
    DWORD size = GetFileSize(hFile, nullptr);
    if (size == INVALID_FILE_SIZE) { CloseHandle(hFile); return ""; }
    std::string buffer(size, '\0');
    DWORD read = 0;
    ReadFile(hFile, &buffer[0], size, &read, nullptr);
    CloseHandle(hFile);
    if (read != size) return "";
    return buffer;
}

inline std::vector<uint8_t> ReadFileToBytes(const std::string& filepath) {
    std::wstring wpath = Utf8ToWide(filepath);
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return {};
    DWORD size = GetFileSize(hFile, nullptr);
    if (size == INVALID_FILE_SIZE) { CloseHandle(hFile); return {}; }
    std::vector<uint8_t> buffer(size);
    DWORD read = 0;
    ReadFile(hFile, buffer.data(), size, &read, nullptr);
    CloseHandle(hFile);
    if (read != size) return {};
    return buffer;
}

inline bool WriteStringToFile(const std::string& filepath, const std::string& content) {
    std::wstring wpath = Utf8ToWide(filepath);
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, content.c_str(), static_cast<DWORD>(content.size()), &written, nullptr);
    CloseHandle(hFile);
    return ok && written == content.size();
}

inline bool CreateDirectoryRecursive(const std::string& path) {
    std::wstring widePath = Utf8ToWide(path);
    int result = SHCreateDirectoryExW(nullptr, widePath.c_str(), nullptr);
    return (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS || result == ERROR_FILE_EXISTS);
}

inline std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm_now;
    localtime_s(&tm_now, &time_t_now);
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}
inline std::string GetTempDirectory() {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    return WideToUtf8(tempPath);
}

inline int GetRandomInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}

inline float GetRandomFloat(float min, float max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
}

inline bool FileExists(const std::string& filepath) {
    std::wstring widePath = Utf8ToWide(filepath);
    DWORD attrs = GetFileAttributesW(widePath.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

inline std::string GetDirectoryFromPath(const std::string& filepath) {
    size_t pos = filepath.find_last_of("\\/");
    if (pos == std::string::npos) return ".";
    return filepath.substr(0, pos);
}

inline std::string GetFileNameFromPath(const std::string& filepath) {
    size_t pos = filepath.find_last_of("\\/");
    if (pos == std::string::npos) return filepath;
    return filepath.substr(pos + 1);
}

inline std::string GetCurrentExecutableDir() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring exePath(buffer);
    size_t pos = exePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return WideToUtf8(exePath.substr(0, pos));
    }
    return ".";
}

}