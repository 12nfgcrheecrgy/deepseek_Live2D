#pragma once

#include <string>
#include <functional>
#include <Windows.h>

class PythonBridge {
public:
    using OutputCallback = std::function<void(const std::string& line)>;

    PythonBridge() = default;
    ~PythonBridge();

    PythonBridge(const PythonBridge&) = delete;
    PythonBridge& operator=(const PythonBridge&) = delete;

    bool RunScript(const std::string& scriptPath, const std::string& args, std::string& output, int timeoutMs = 30000);
    bool RunScriptAsync(const std::string& scriptPath, const std::string& args, OutputCallback onOutput = nullptr, int timeoutMs = 30000);

    bool StartPersistentProcess(const std::string& scriptPath, const std::string& args, int timeoutMs = 60000);
    bool StopPersistentProcess(int timeoutMs = 10000);
    bool SendRequest(const std::string& requestJson, std::string& responseJson, int timeoutMs = 30000);
    bool IsPersistentAlive();
    bool RestartPersistentProcess();
    bool IsPersistentReady() const { return m_persistentReady; }

    static std::string GetPythonExe();

private:
    bool CreateChildProcess(const std::wstring& cmdLine, HANDLE hStdOutWrite, HANDLE hStdErrWrite, HANDLE& hProcess, HANDLE& hThread);
    bool CreateChildProcessWithStdin(const std::wstring& cmdLine,
                                      HANDLE hStdInRead, HANDLE hStdOutWrite, HANDLE hStdErrWrite,
                                      HANDLE& hProcess, HANDLE& hThread);
    std::string ReadPipeOutput(HANDLE hPipe, DWORD timeoutMs);

    HANDLE m_hChildProcess = nullptr;
    HANDLE m_hChildThread = nullptr;

    HANDLE m_hPersistentProcess = nullptr;
    HANDLE m_hPersistentThread = nullptr;
    HANDLE m_hPersistentStdinWrite = nullptr;
    HANDLE m_hPersistentStdoutRead = nullptr;
    HANDLE m_hPersistentStderrRead = nullptr;
    std::string m_persistentScriptPath;
    std::string m_persistentArgs;
    bool m_persistentReady = false;
    int m_restartCount = 0;
    static const int MAX_RESTART_COUNT = 3;
};