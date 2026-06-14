#include "PythonBridge.h"
#include "utils.h"
#include "LAppPal.h"
#include "nlohmann/json.hpp"
#include <vector>
#include <algorithm>

PythonBridge::~PythonBridge() {
    if (m_persistentReady || m_hPersistentProcess) {
        StopPersistentProcess(3000);
    }
    if (m_hChildThread) {
        CloseHandle(m_hChildThread);
        m_hChildThread = nullptr;
    }
    if (m_hChildProcess) {
        CloseHandle(m_hChildProcess);
        m_hChildProcess = nullptr;
    }
}

std::string PythonBridge::GetPythonExe() {
    std::string exeDir = utils::GetCurrentExecutableDir();
    std::string projectRoot = exeDir;
    for (int i = 0; i < 3; i++) {
        std::string parent = projectRoot.substr(0, projectRoot.find_last_of("\\/"));
        if (parent.empty() || parent == projectRoot) break;
        projectRoot = parent;
        std::string venvPath = projectRoot + "\\venv_tts\\Scripts\\python.exe";
        if (utils::FileExists(venvPath)) {
            return venvPath;
        }
    }

    if (!exeDir.empty()) {
        std::string localVenv = exeDir + "\\..\\venv_tts\\Scripts\\python.exe";
        if (utils::FileExists(localVenv)) {
            wchar_t fullPath[MAX_PATH];
            std::wstring wLocalVenv = utils::Utf8ToWide(localVenv);
            if (GetFullPathNameW(wLocalVenv.c_str(), MAX_PATH, fullPath, nullptr) > 0) {
                return utils::WideToUtf8(fullPath);
            }
        }
    }
    const wchar_t* candidates[] = {
        L"python.exe",
        L"python3.exe",
        L"C:\\Python312\\python.exe",
        L"C:\\Python311\\python.exe",
        L"C:\\Python310\\python.exe",
        L"C:\\Python39\\python.exe",
        L"C:\\Python38\\python.exe",
    };

    for (const auto& candidate : candidates) {
        std::wstring fullPath;
        wchar_t buffer[MAX_PATH];
        DWORD len = SearchPathW(nullptr, candidate, L".exe", MAX_PATH, buffer, nullptr);
        if (len > 0 && len < MAX_PATH) {
            std::string path = utils::WideToUtf8(buffer);
            if (utils::FileExists(path)) {
                return path;
            }
        }
        if (utils::FileExists(utils::WideToUtf8(candidate))) {
            return utils::WideToUtf8(candidate);
        }
    }

    std::wstring pathEnv;
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring searchPath = std::wstring(sysDir) + L"\\python.exe";
    if (GetFileAttributesW(searchPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return utils::WideToUtf8(searchPath);
    }

    return "python.exe";
}

bool PythonBridge::CreateChildProcess(const std::wstring& cmdLine, HANDLE hStdOutWrite, HANDLE hStdErrWrite, HANDLE& hProcess, HANDLE& hThread) {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(L'\0');

    BOOL success = CreateProcessW(
        nullptr,
        cmdLineBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (!success) {
        DWORD err = GetLastError();
        LAppPal::PrintLog("[PythonBridge] CreateProcessW failed, error=%lu", err);
        return false;
    }

    CloseHandle(pi.hThread);
    pi.hThread = nullptr;

    hProcess = pi.hProcess;
    hThread = nullptr;
    return true;
}

bool PythonBridge::CreateChildProcessWithStdin(const std::wstring& cmdLine,
                                                HANDLE hStdInRead, HANDLE hStdOutWrite, HANDLE hStdErrWrite,
                                                HANDLE& hProcess, HANDLE& hThread) {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = hStdInRead;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(L'\0');

    BOOL success = CreateProcessW(
        nullptr,
        cmdLineBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (!success) {
        DWORD err = GetLastError();
        LAppPal::PrintLog("[PythonBridge] CreateProcessW (with stdin) failed, error=%lu", err);
        return false;
    }

    CloseHandle(pi.hThread);
    pi.hThread = nullptr;

    hProcess = pi.hProcess;
    hThread = nullptr;
    return true;
}

std::string PythonBridge::ReadPipeOutput(HANDLE hPipe, DWORD timeoutMs) {
    std::string result;
    char buffer[4096];
    DWORD bytesRead;
    DWORD bytesAvailable;
    DWORD startTime = GetTickCount();

    while (true) {
        DWORD elapsed = GetTickCount() - startTime;
        if (elapsed > static_cast<DWORD>(timeoutMs)) {
            break;
        }

        if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            break;
        }

        if (bytesAvailable > 0) {
            DWORD toRead = std::min(bytesAvailable, static_cast<DWORD>(sizeof(buffer) - 1));
            if (ReadFile(hPipe, buffer, toRead, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result += buffer;
            }
        } else {
            DWORD exitCode;
            if (m_hChildProcess) {
                if (GetExitCodeProcess(m_hChildProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                    bytesAvailable = 0;
                    if (PeekNamedPipe(hPipe, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0) {
                        continue;
                    }
                    break;
                }
            }
            Sleep(50);
        }
    }

    return result;
}

bool PythonBridge::RunScript(const std::string& scriptPath, const std::string& args, std::string& output, int timeoutMs) {
    output.clear();

    LAppPal::PrintLog("[PythonBridge] RunScript: %s", scriptPath.c_str());

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE hStdOutRead = nullptr;
    HANDLE hStdOutWrite = nullptr;
    HANDLE hStdErrRead = nullptr;
    HANDLE hStdErrWrite = nullptr;

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
        LAppPal::PrintLog("[PythonBridge] CreatePipe for stdout failed");
        return false;
    }

    if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
        LAppPal::PrintLog("[PythonBridge] SetHandleInformation for stdout failed");
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return false;
    }

    if (!CreatePipe(&hStdErrRead, &hStdErrWrite, &saAttr, 0)) {
        LAppPal::PrintLog("[PythonBridge] CreatePipe for stderr failed");
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return false;
    }

    if (!SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0)) {
        LAppPal::PrintLog("[PythonBridge] SetHandleInformation for stderr failed");
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrRead);
        CloseHandle(hStdErrWrite);
        return false;
    }

    std::string pythonExe = GetPythonExe();
    std::wstring cmdLine = L"\"" + utils::Utf8ToWide(pythonExe) + L"\" \"" + utils::Utf8ToWide(scriptPath) + L"\" " + utils::Utf8ToWide(args);

    LAppPal::PrintLog("[PythonBridge] CmdLine (utf8): %s %s [...args]",
                      pythonExe.c_str(), scriptPath.c_str());

    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;
    if (!CreateChildProcess(cmdLine, hStdOutWrite, hStdErrWrite, hProcess, hThread)) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrRead);
        CloseHandle(hStdErrWrite);
        return false;
    }

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);
    hStdOutWrite = nullptr;
    hStdErrWrite = nullptr;

    m_hChildProcess = hProcess;
    m_hChildThread = hThread;

    output = ReadPipeOutput(hStdOutRead, timeoutMs);

    std::string stderrOutput = ReadPipeOutput(hStdErrRead, 1000);
    if (!stderrOutput.empty()) {
        LAppPal::PrintLog("[PythonBridge] stderr: %s", stderrOutput.c_str());
    }

    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);

    if (m_hChildProcess) {
        DWORD exitCode;
        WaitForSingleObject(m_hChildProcess, timeoutMs > 0 ? timeoutMs : INFINITE);
        GetExitCodeProcess(m_hChildProcess, &exitCode);
        LAppPal::PrintLog("[PythonBridge] Process exit code: %lu", exitCode);
        CloseHandle(m_hChildProcess);
        m_hChildProcess = nullptr;
    }

    LAppPal::PrintLog("[PythonBridge] RunScript result: output length=%zu", output.size());
    return !output.empty();
}

bool PythonBridge::RunScriptAsync(const std::string& scriptPath, const std::string& args, OutputCallback onOutput, int timeoutMs) {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE hStdOutRead = nullptr;
    HANDLE hStdOutWrite = nullptr;
    HANDLE hStdErrRead = nullptr;
    HANDLE hStdErrWrite = nullptr;

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
        return false;
    }

    if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return false;
    }

    if (!CreatePipe(&hStdErrRead, &hStdErrWrite, &saAttr, 0)) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return false;
    }

    if (!SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrRead);
        CloseHandle(hStdErrWrite);
        return false;
    }

    std::string pythonExe = GetPythonExe();
    std::wstring cmdLine = L"\"" + utils::Utf8ToWide(pythonExe) + L"\" \"" + utils::Utf8ToWide(scriptPath) + L"\" " + utils::Utf8ToWide(args);

    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;
    if (!CreateChildProcess(cmdLine, hStdOutWrite, hStdErrWrite, hProcess, hThread)) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrRead);
        CloseHandle(hStdErrWrite);
        return false;
    }

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);
    hStdOutWrite = nullptr;
    hStdErrWrite = nullptr;

    m_hChildProcess = hProcess;
    m_hChildThread = hThread;

    std::string output = ReadPipeOutput(hStdOutRead, timeoutMs);

    if (onOutput && !output.empty()) {
        onOutput(output);
    }

    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);

    if (m_hChildProcess) {
        DWORD exitCode;
        WaitForSingleObject(m_hChildProcess, timeoutMs > 0 ? timeoutMs : INFINITE);
        GetExitCodeProcess(m_hChildProcess, &exitCode);
        CloseHandle(m_hChildProcess);
        m_hChildProcess = nullptr;
    }

    return !output.empty();
}

bool PythonBridge::StartPersistentProcess(const std::string& scriptPath, const std::string& args, int timeoutMs) {
    LAppPal::PrintLog("[PythonBridge] StartPersistentProcess: %s", scriptPath.c_str());

    if (m_persistentReady) {
        LAppPal::PrintLog("[PythonBridge] Persistent process already running");
        return true;
    }

    m_persistentScriptPath = scriptPath;
    m_persistentArgs = args;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE hStdInRead = nullptr;
    HANDLE hStdInWrite = nullptr;
    HANDLE hStdOutRead = nullptr;
    HANDLE hStdOutWrite = nullptr;
    HANDLE hStdErrRead = nullptr;
    HANDLE hStdErrWrite = nullptr;

    if (!CreatePipe(&hStdInRead, &hStdInWrite, &saAttr, 0)) {
        LAppPal::PrintLog("[PythonBridge] CreatePipe for stdin failed");
        return false;
    }
    if (!SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0)) {
        LAppPal::PrintLog("[PythonBridge] SetHandleInformation for stdin write failed");
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        return false;
    }

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
        LAppPal::PrintLog("[PythonBridge] CreatePipe for stdout failed");
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        return false;
    }
    if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
        LAppPal::PrintLog("[PythonBridge] SetHandleInformation for stdout read failed");
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return false;
    }

    if (!CreatePipe(&hStdErrRead, &hStdErrWrite, &saAttr, 0)) {
        LAppPal::PrintLog("[PythonBridge] CreatePipe for stderr failed");
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return false;
    }
    if (!SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0)) {
        LAppPal::PrintLog("[PythonBridge] SetHandleInformation for stderr read failed");
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrRead);
        CloseHandle(hStdErrWrite);
        return false;
    }

    std::string pythonExe = GetPythonExe();
    std::wstring cmdLine = L"\"" + utils::Utf8ToWide(pythonExe) + L"\" \"" + utils::Utf8ToWide(scriptPath) + L"\" " + utils::Utf8ToWide(args);

    LAppPal::PrintLog("[PythonBridge] Persistent CmdLine: %s %s",
                      pythonExe.c_str(), scriptPath.c_str());

    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;
    if (!CreateChildProcessWithStdin(cmdLine, hStdInRead, hStdOutWrite, hStdErrWrite, hProcess, hThread)) {
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrRead);
        CloseHandle(hStdErrWrite);
        return false;
    }

    CloseHandle(hStdInRead);
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);
    hStdInRead = nullptr;
    hStdOutWrite = nullptr;
    hStdErrWrite = nullptr;

    m_hPersistentProcess = hProcess;
    m_hPersistentThread = hThread;
    m_hPersistentStdinWrite = hStdInWrite;
    m_hPersistentStdoutRead = hStdOutRead;
    m_hPersistentStderrRead = hStdErrRead;

    // Read line-by-line so library log noise that leaks into stdout doesn't break JSON parsing.
    DWORD startTime = GetTickCount();
    std::string buffer;
    char readBuf[4096];
    DWORD bytesRead, bytesAvailable;

    while (true) {
        DWORD elapsed = GetTickCount() - startTime;
        if (elapsed > static_cast<DWORD>(timeoutMs)) {
            LAppPal::PrintLog("[PythonBridge] No ready response from persistent process within timeout");
            std::string stderrOut = ReadPipeOutput(m_hPersistentStderrRead, 2000);
            if (!stderrOut.empty()) {
                LAppPal::PrintLog("[PythonBridge] Stderr: %s", stderrOut.c_str());
            }
            StopPersistentProcess(1);
            return false;
        }

        DWORD exitCode;
        if (GetExitCodeProcess(m_hPersistentProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            LAppPal::PrintLog("[PythonBridge] Persistent process exited early, code=%lu", exitCode);
            std::string stderrOut = ReadPipeOutput(m_hPersistentStderrRead, 2000);
            if (!stderrOut.empty()) {
                LAppPal::PrintLog("[PythonBridge] Stderr: %s", stderrOut.c_str());
            }
            StopPersistentProcess(1);
            return false;
        }

        if (!PeekNamedPipe(m_hPersistentStdoutRead, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            break;
        }

        if (bytesAvailable > 0) {
            DWORD toRead = std::min(bytesAvailable, static_cast<DWORD>(sizeof(readBuf) - 1));
            if (ReadFile(m_hPersistentStdoutRead, readBuf, toRead, &bytesRead, nullptr) && bytesRead > 0) {
                readBuf[bytesRead] = '\0';
                buffer += readBuf;

                // Extract complete lines
                size_t pos = 0;
                while ((pos = buffer.find('\n')) != std::string::npos) {
                    std::string line = buffer.substr(0, pos);
                    buffer.erase(0, pos + 1);

                    // Trim \r and whitespace
                    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                    while (!line.empty() && (line.front() == ' ')) line.erase(0, 1);

                    if (line.empty()) continue;

                    // Try to parse as JSON
                    if (line.front() == '{' || line.front() == '[') {
                        try {
                            auto j = nlohmann::json::parse(line);
                            if (j.contains("status")) {
                                std::string status = j.value("status", "");
                                if (status == "ready") {
                                    m_persistentReady = true;
                                    m_restartCount = 0;
                                    LAppPal::PrintLog("[PythonBridge] Persistent process ready!");
                                    if (j.contains("load_time_ms")) {
                                        LAppPal::PrintLog("[PythonBridge] Model load time: %.0f ms",
                                                          j["load_time_ms"].get<float>());
                                    }
                                    return true;
                                } else if (status == "error") {
                                    std::string err = j.value("error", "unknown");
                                    LAppPal::PrintLog("[PythonBridge] Persistent process error: %s", err.c_str());
                                    StopPersistentProcess(1);
                                    return false;
                                }
                            }
                        } catch (const std::exception&) {
                            // Not valid JSON, treat as log line
                        }
                    }
                    // Log non-JSON stdout for debugging (only first 200 chars to avoid spam)
                    if (line.size() > 200) line = line.substr(0, 200) + "...";
                    LAppPal::PrintLog("[PythonBridge] Persistent process stdout: %s", line.c_str());
                }
            }
        } else {
            Sleep(50);
        }
    }

    StopPersistentProcess(1);
    return false;
}

bool PythonBridge::StopPersistentProcess(int timeoutMs) {
    LAppPal::PrintLog("[PythonBridge] StopPersistentProcess");

    m_persistentReady = false;

    if (m_hPersistentStdinWrite) {
        std::string shutdownCmd = "{\"command\":\"shutdown\"}\n";
        DWORD written;
        WriteFile(m_hPersistentStdinWrite, shutdownCmd.c_str(),
                  static_cast<DWORD>(shutdownCmd.size()), &written, nullptr);
        CloseHandle(m_hPersistentStdinWrite);
        m_hPersistentStdinWrite = nullptr;
    }

    if (m_hPersistentStdoutRead) {
        std::string remaining = ReadPipeOutput(m_hPersistentStdoutRead, timeoutMs > 0 ? timeoutMs : 1000);
        if (!remaining.empty()) {
            LAppPal::PrintLog("[PythonBridge] Shutdown response: %s", remaining.c_str());
        }
        CloseHandle(m_hPersistentStdoutRead);
        m_hPersistentStdoutRead = nullptr;
    }

    if (m_hPersistentStderrRead) {
        CloseHandle(m_hPersistentStderrRead);
        m_hPersistentStderrRead = nullptr;
    }

    if (m_hPersistentProcess) {
        DWORD exitCode;
        if (timeoutMs > 0) {
            WaitForSingleObject(m_hPersistentProcess, timeoutMs);
        }
        GetExitCodeProcess(m_hPersistentProcess, &exitCode);
        TerminateProcess(m_hPersistentProcess, 0);
        CloseHandle(m_hPersistentProcess);
        m_hPersistentProcess = nullptr;
        LAppPal::PrintLog("[PythonBridge] Persistent process terminated");
    }

    if (m_hPersistentThread) {
        CloseHandle(m_hPersistentThread);
        m_hPersistentThread = nullptr;
    }

    return true;
}

bool PythonBridge::SendRequest(const std::string& requestJson, std::string& responseJson, int timeoutMs) {
    responseJson.clear();

    if (!m_persistentReady || !m_hPersistentStdinWrite || !m_hPersistentStdoutRead) {
        LAppPal::PrintLog("[PythonBridge] SendRequest: persistent process not ready");
        return false;
    }

    if (!IsPersistentAlive()) {
        LAppPal::PrintLog("[PythonBridge] SendRequest: persistent process died");
        m_persistentReady = false;
        return false;
    }

    std::string message = requestJson + "\n";
    DWORD written;
    if (!WriteFile(m_hPersistentStdinWrite, message.c_str(),
                   static_cast<DWORD>(message.size()), &written, nullptr)) {
        LAppPal::PrintLog("[PythonBridge] SendRequest: WriteFile failed");
        m_persistentReady = false;
        return false;
    }

    DWORD startTime = GetTickCount();
    std::string response;
    char buffer[4096];
    DWORD bytesRead;
    DWORD bytesAvailable;

    while (true) {
        DWORD elapsed = GetTickCount() - startTime;
        if (elapsed > static_cast<DWORD>(timeoutMs)) {
            LAppPal::PrintLog("[PythonBridge] SendRequest: timeout");
            break;
        }

        DWORD exitCode;
        if (GetExitCodeProcess(m_hPersistentProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            m_persistentReady = false;
            LAppPal::PrintLog("[PythonBridge] SendRequest: process exited during request, code=%lu", exitCode);
            std::string stderrRemaining = ReadPipeOutput(m_hPersistentStderrRead, 1000);
            if (!stderrRemaining.empty()) {
                LAppPal::PrintLog("[PythonBridge] Stderr: %s", stderrRemaining.c_str());
            }
            return false;
        }

        if (!PeekNamedPipe(m_hPersistentStdoutRead, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            LAppPal::PrintLog("[PythonBridge] SendRequest: PeekNamedPipe failed");
            m_persistentReady = false;
            return false;
        }

        if (bytesAvailable > 0) {
            DWORD toRead = std::min(bytesAvailable, static_cast<DWORD>(sizeof(buffer) - 1));
            if (ReadFile(m_hPersistentStdoutRead, buffer, toRead, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                response += buffer;

                // Process complete lines, skipping any non-JSON log noise
                size_t pos = 0;
                while ((pos = response.find('\n')) != std::string::npos) {
                    std::string line = response.substr(0, pos);
                    response.erase(0, pos + 1);

                    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                    while (!line.empty() && (line.front() == ' ')) line.erase(0, 1);

                    if (line.empty()) continue;

                    if (line.front() == '{' || line.front() == '[') {
                        try {
                            auto j = nlohmann::json::parse(line);
                            // Valid JSON response from the server
                            responseJson = line;
                            return true;
                        } catch (const std::exception&) {
                            // Not valid JSON, ignore log lines
                        }
                    }
                    // Non-JSON stdout line (residual log noise), ignore
                }
            }
        } else {
            Sleep(20);
        }
    }

    if (!response.empty()) {
        // Try to parse remaining buffer as JSON
        std::string line = response;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        while (!line.empty() && (line.front() == ' ')) line.erase(0, 1);
        if (!line.empty() && (line.front() == '{' || line.front() == '[')) {
            try {
                auto j = nlohmann::json::parse(line);
                responseJson = line;
                return true;
            } catch (const std::exception&) {
            }
        }
    }

    return false;
}

bool PythonBridge::IsPersistentAlive() {
    if (!m_hPersistentProcess) {
        return false;
    }
    DWORD exitCode;
    if (GetExitCodeProcess(m_hPersistentProcess, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }
    return false;
}

bool PythonBridge::RestartPersistentProcess() {
    LAppPal::PrintLog("[PythonBridge] RestartPersistentProcess, attempt #%d", m_restartCount + 1);

    if (m_restartCount >= MAX_RESTART_COUNT) {
        LAppPal::PrintLog("[PythonBridge] Max restart count reached, giving up");
        return false;
    }

    StopPersistentProcess(1000);
    m_restartCount++;

    Sleep(1000);

    bool result = StartPersistentProcess(m_persistentScriptPath, m_persistentArgs, 60000);
    if (result) {
        LAppPal::PrintLog("[PythonBridge] RestartPersistentProcess succeeded");
    } else {
        LAppPal::PrintLog("[PythonBridge] RestartPersistentProcess failed");
    }

    return result;
}