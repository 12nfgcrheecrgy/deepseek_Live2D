#include <Windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <CommCtrl.h>
#include <shellapi.h>
#include <mmsystem.h>

#include "Config.h"
#include "CompanionWindow.h"
#include "LAppDelegate.h"
#include "LAppView.h"
#include "Live2DManager.h"
#include "LAppModel.h"
#include "LAppPal.h"
#include "AudioManager.h"
#include "DeepSeekClient.h"
#include "TTSManager.h"
#include "OCRManager.h"
#include "ExpressionManager.h"
#include "MotionManager.h"
#include "ChatDialog.h"
#include "utils.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")

static LONG WINAPI MyExceptionHandler(EXCEPTION_POINTERS* exInfo) {
    DWORD code = exInfo->ExceptionRecord->ExceptionCode;
    void* addr = exInfo->ExceptionRecord->ExceptionAddress;
    LAppPal::PrintLog("[main] CRASH! Exception code: 0x%08X at address: %p", code, addr);

    if (code == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR attempted = exInfo->ExceptionRecord->ExceptionInformation[1];
        LAppPal::PrintLog("[main] Access violation: attempted to %s address 0x%p",
                          exInfo->ExceptionRecord->ExceptionInformation[0] == 0 ? "read from" : "write to",
                          (void*)attempted);
    }

    MessageBoxW(nullptr,
                L"A fatal error occurred in Live2D Companion.\nCheck Live2D_debug.log for details.",
                L"Live2D Companion - Crash", MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_isTalking = false;
static std::atomic<bool> g_ocrTimerRunning = false;

// Forward declarations
static void DoChat(const std::string& userMessage);

static std::unique_ptr<AudioManager> g_audioManager;
static std::unique_ptr<DeepSeekClient> g_deepSeekClient;
static std::unique_ptr<TTSManager> g_ttsManager;
static std::unique_ptr<OCRManager> g_ocrManager;
static std::unique_ptr<ExpressionManager> g_expressionManager;
static std::unique_ptr<MotionManager> g_motionManager;
static std::unique_ptr<ChatDialog> g_chatDialog;

LAppDelegate* g_appDelegate = nullptr;

static HMENU CreateContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1001, L"Chat... (C)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | (g_ocrManager && g_ocrManager->IsEnabled() ? MF_CHECKED : 0),
                1004, L"Toggle OCR");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 1005, L"Settings...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 1006, L"Quit");
    return hMenu;
}

static void LaunchChatGUI() {
    std::string exeDir = utils::GetCurrentExecutableDir();
    std::string chatGuiPath = exeDir + "\\python\\chat_gui.py";

    if (!utils::FileExists(chatGuiPath)) {
        LAppPal::PrintLog("[main] chat_gui.py not found: %s", chatGuiPath.c_str());
        return;
    }

    std::string pythonExe = PythonBridge::GetPythonExe();
    std::wstring cmdLine = L"\"" + utils::Utf8ToWide(pythonExe) + L"\" \""
                         + utils::Utf8ToWide(chatGuiPath) + L"\"";

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(L'\0');

    BOOL success = CreateProcessW(
        nullptr, cmdLineBuf.data(),
        nullptr, nullptr, FALSE,
        DETACHED_PROCESS,
        nullptr, nullptr, &si, &pi
    );

    if (success) {
        LAppPal::PrintLog("[main] Chat GUI launched (PID: %lu)", pi.dwProcessId);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        LAppPal::PrintLog("[main] Failed to launch Chat GUI, error=%lu", GetLastError());
    }
}

static std::string GetConfigPath() {
    std::string exeDir = utils::GetCurrentExecutableDir();
    return exeDir + "\\config.json";
}

static void InitializeConfig() {
    std::string configPath = GetConfigPath();
    Config::Load(configPath);

    auto& cfg = Config::GetInstance();

    std::string exeDir = utils::GetCurrentExecutableDir();
    std::string pythonScriptPath = exeDir + "\\python\\deepseek_api.py";

    g_deepSeekClient->SetConfig(
        cfg.GetDeepSeek().api_key,
        cfg.GetDeepSeek().api_base,
        cfg.GetDeepSeek().model,
        cfg.GetDeepSeek().max_tokens,
        cfg.GetDeepSeek().temperature,
        cfg.GetDeepSeek().system_prompt
    );

    g_ttsManager->SetConfig(
        cfg.GetTTS().voice,
        cfg.GetTTS().rate,
        cfg.GetTTS().pitch,
        cfg.GetTTS().voice_happy,
        cfg.GetTTS().voice_surprised,
        cfg.GetTTS().voice_normal,
        cfg.GetTTS().rate_happy,
        cfg.GetTTS().rate_surprised,
        cfg.GetTTS().pitch_happy,
        cfg.GetTTS().pitch_surprised
    );

    if (cfg.GetTTS().engine == "indextts2") {
        g_ttsManager->SetEngine(TTSManager::Engine::IndexTTS2);
        g_ttsManager->SetIndexTTS2Config(
            cfg.GetTTS().indextts2_model_dir,
            cfg.GetTTS().indextts2_config,
            cfg.GetTTS().indextts2_reference_audio,
            cfg.GetTTS().indextts2_use_fp16,
            cfg.GetTTS().indextts2_use_openvino,
            cfg.GetTTS().indextts2_openvino_device
        );
        LAppPal::PrintLog("[main] TTS engine set to IndexTTS2");
    } else if (cfg.GetTTS().engine == "sovits") {
        g_ttsManager->SetEngine(TTSManager::Engine::SoVITS);
        g_ttsManager->SetSoVITSConfig(
            cfg.GetTTS().sovits_model_path,
            cfg.GetTTS().sovits_config_path,
            cfg.GetTTS().sovits_diffusion_path,
            cfg.GetTTS().sovits_diffusion_config,
            cfg.GetTTS().sovits_reference_audio,
            cfg.GetTTS().sovits_fp16,
            cfg.GetTTS().sovits_gpt_weights
        );
        LAppPal::PrintLog("[main] TTS engine set to SoVITS");
    } else {
        LAppPal::PrintLog("[main] TTS engine set to Edge");
    }

    g_ocrManager->SetConfig(
        cfg.GetOCR().language,
        cfg.GetOCR().interval_seconds,
        cfg.GetOCR().enabled
    );

    // Create chat dialog
    g_chatDialog = std::make_unique<ChatDialog>();
    g_chatDialog->SetSendCallback([](const std::string& msg) {
        std::thread([msg]() { DoChat(msg); }).detach();
    });
}

static void DoChat(const std::string& userMessage) {
    LAppPal::PrintLog("[DoChat] === ENTER === message='%s'", userMessage.c_str());

    // Guard: check all managers are ready
    if (!g_deepSeekClient || !g_ttsManager || !g_appDelegate) {
        LAppPal::PrintLog("[DoChat] ERROR: managers not ready (deepseek=%d tts=%d app=%d)",
                          g_deepSeekClient != nullptr, g_ttsManager != nullptr, g_appDelegate != nullptr);
        if (g_chatDialog) {
            g_chatDialog->AddMessage("Furina", "Just a moment, I'm still waking up...");
        }
        return;
    }

    if (g_isTalking.load()) {
        LAppPal::PrintLog("[DoChat] Already talking, returning");
        return;
    }
    g_isTalking.store(true);

    // OCR screen content for context
    std::string screenContent;
    if (g_ocrManager && g_ocrManager->IsEnabled()) {
        screenContent = g_ocrManager->GetLastText();
        if (screenContent.empty()) {
            g_ocrManager->CaptureAndOCR();
            screenContent = g_ocrManager->GetLastText();
        }
    }

    LAppPal::PrintLog("[DoChat] Calling DeepSeek Chat...");
    std::string reply;
    bool success = g_deepSeekClient->Chat(userMessage, screenContent, reply);
    LAppPal::PrintLog("[DoChat] DeepSeek Chat result: success=%d, reply length=%zu",
                      success, reply.size());

    if (!success || reply.empty()) {
        if (g_chatDialog) {
            g_chatDialog->AddMessage("Furina",
                "Sorry master, my brain is a bit fuzzy right now... try again later, okay?~");
        }
        g_isTalking.store(false);
        return;
    }

    // Detect emotion from reply
    TTSManager::Tone tone = TTSManager::Tone::Normal;
    std::string replyLower = reply;
    for (auto& c : replyLower) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

    if (replyLower.find("!") != std::string::npos ||
        replyLower.find("wow") != std::string::npos ||
        replyLower.find("what") != std::string::npos) {
        tone = TTSManager::Tone::Surprised;
    } else if (replyLower.find("ha") != std::string::npos ||
               replyLower.find("~") != std::string::npos ||
               replyLower.find("\xe2\x98\xba") != std::string::npos) {
        tone = TTSManager::Tone::Happy;
    }

    // Show reply in chat
    if (g_chatDialog) {
        g_chatDialog->AddMessage("Furina", reply);
    }

    auto ttsStart = std::chrono::high_resolution_clock::now();
    LAppPal::PrintLog("[DoChat] Calling TTS Speak...");
    std::string audioFile;
    if (g_ttsManager->Speak(reply, tone, audioFile)) {
        auto ttsEnd = std::chrono::high_resolution_clock::now();
        float ttsLatency = std::chrono::duration<float, std::milli>(ttsEnd - ttsStart).count();
        LAppPal::PrintLog("[Perf] TTS latency: %.1f ms", ttsLatency);
        LAppPal::PrintLog("[DoChat] TTS success, audioFile=%s", audioFile.c_str());

        // Auto-trigger character animations
        LAppModel* model = g_appDelegate->GetModelManager()->GetModel(0);
        if (model) {
            switch (tone) {
                case TTSManager::Tone::Happy:
                    model->SetAnimState(AnimState::Happy);
                    break;
                case TTSManager::Tone::Surprised:
                    model->SetAnimState(AnimState::Surprised);
                    break;
                default:
                    model->SetAnimState(AnimState::Talking);
                    break;
            }
        }

        if (g_motionManager) {
            g_motionManager->StartMotion(MotionManager::MotionType::Talk,
                                         MotionManager::Priority::High);
        }

        switch (tone) {
            case TTSManager::Tone::Happy:
                g_expressionManager->SetExpression(ExpressionManager::ExpressionType::Happy);
                break;
            case TTSManager::Tone::Surprised:
                g_expressionManager->SetExpression(ExpressionManager::ExpressionType::Surprised);
                break;
            default:
                g_expressionManager->SetExpression(ExpressionManager::ExpressionType::Neutral);
                break;
        }

        LAppPal::PrintLog("[DoChat] Playing audio asynchronously: %s", audioFile.c_str());
        bool playOk = g_audioManager->Play(audioFile, true, []() {
            if (g_motionManager) {
                g_motionManager->StopMotion();
            }
            g_expressionManager->SetExpression(ExpressionManager::ExpressionType::Neutral);
            if (g_appDelegate && g_appDelegate->GetModelManager()) {
                LAppModel* m = g_appDelegate->GetModelManager()->GetModel(0);
                if (m) m->SetAnimState(AnimState::Idle);
            }
        });
        LAppPal::PrintLog("[DoChat] Audio playback %s", playOk ? "started" : "FAILED");
    } else {
        LAppPal::PrintLog("[DoChat] TTS Speak failed");
        if (g_chatDialog) {
            g_chatDialog->AddMessage("Furina",
                "(Ahem, my voice failed... but I'm still listening!)");
        }
    }

    g_isTalking.store(false);
    LAppPal::PrintLog("[DoChat] Exiting");
}

static void HandleContextMenu(HWND hwnd, int x, int y) {
    HMENU hMenu = CreateContextMenu();
    POINT pt = {x, y};
    ClientToScreen(hwnd, &pt);

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY,
                              pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
        case 1001:  // Chat
            if (g_chatDialog) {
                g_chatDialog->Show();
            }
            break;
        case 1004:  // Toggle OCR
            if (g_ocrManager) {
                g_ocrManager->SetEnabled(!g_ocrManager->IsEnabled());
            }
            break;
        case 1005:  // Settings
            {
                std::string configPath = GetConfigPath();
                ShellExecuteW(nullptr, L"open", L"notepad.exe",
                              utils::Utf8ToWide(configPath).c_str(), nullptr, SW_SHOW);
            }
            break;
        case 1006:  // Quit
            g_running.store(false);
            PostQuitMessage(0);
            break;
    }
}

static void HandleKeyPress(int key, bool pressed) {
    if (!pressed) return;

    if (key == 'C' || key == 'c') {
        if (g_chatDialog) {
            g_chatDialog->Show();
        }
    }
}

static void HandleMouseClick(int button, int action, int x, int y) {
    if (button == 1 && action == 1) {
        if (g_appDelegate && g_appDelegate->GetWindow()) {
            HWND hwnd = g_appDelegate->GetWindow()->GetHwnd();
            HandleContextMenu(hwnd, x, y);
        }
    }

    if (button == 0 && action == 1) {
        if (g_appDelegate && g_appDelegate->GetModelManager()) {
            g_appDelegate->GetModelManager()->OnTap(
                static_cast<float>(x),
                static_cast<float>(y)
            );
        }
    }
}

static void OCRTimerLoop(int intervalSeconds) {
    while (g_running.load()) {
        if (g_ocrManager && g_ocrManager->IsEnabled()) {
            g_ocrManager->CaptureAndOCR();
        }

        for (int i = 0; i < intervalSeconds * 10 && g_running.load(); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\Live2DCompanion_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hWnd = FindWindowW(L"Live2DCompanionWindow", nullptr);
        if (hWnd) {
            ShowWindow(hWnd, SW_SHOW);
            SetForegroundWindow(hWnd);
            FlashWindow(hWnd, TRUE);
        }
        CloseHandle(hMutex);
        return 0;
    }

    SetUnhandledExceptionFilter(MyExceptionHandler);
    LAppPal::PrintLog("[main] === Live2DCompanion starting ===");

    std::string configPath = GetConfigPath();
    LAppPal::PrintLog("[main] Config path: %s", configPath.c_str());

    if (!Config::Load(configPath)) {
        LAppPal::PrintLog("[main] Failed to load config, using defaults");
    }

    auto& cfg = Config::GetInstance();
    LAppPal::PrintLog("[main] Config loaded: model=%s", cfg.GetModel().name.c_str());

    g_audioManager = std::make_unique<AudioManager>();
    g_deepSeekClient = std::make_unique<DeepSeekClient>();
    g_ttsManager = std::make_unique<TTSManager>();
    g_ocrManager = std::make_unique<OCRManager>();
    g_expressionManager = std::make_unique<ExpressionManager>();
    g_motionManager = std::make_unique<MotionManager>();

    InitializeConfig();

    if (g_ttsManager->GetEngine() == TTSManager::Engine::IndexTTS2) {
        LAppPal::PrintLog("[main] Starting IndexTTS2 server with GUI loader...");
        g_ttsManager->StartLoaderGUI("IndexTTS2");
        std::thread([]() {
            bool ok = g_ttsManager->InitializeIndexTTS2(90, true);
            if (ok) {
                LAppPal::PrintLog("[main] IndexTTS2 server ready");
            } else {
                LAppPal::PrintLog("[main] IndexTTS2 server failed to start");
            }
            g_ttsManager->TerminateLoaderGUI();
        }).detach();
    } else if (g_ttsManager->GetEngine() == TTSManager::Engine::SoVITS) {
        LAppPal::PrintLog("[main] Starting SoVITS server with GUI loader...");
        g_ttsManager->StartLoaderGUI("SoVITS");
        std::thread([]() {
            bool ok = g_ttsManager->InitializeSoVITS(300, true);
            if (ok) {
                LAppPal::PrintLog("[main] SoVITS server ready");
            } else {
                LAppPal::PrintLog("[main] SoVITS server failed to start");
            }
            g_ttsManager->TerminateLoaderGUI();
        }).detach();
    }

    LAppDelegate app;
    g_appDelegate = &app;

    std::string modelDir = cfg.GetModel().directory;
    std::string modelName = cfg.GetModel().name;

    std::string exeDir = utils::GetCurrentExecutableDir();
    std::string fullModelDir = exeDir + "\\" + modelDir;
    LAppPal::PrintLog("[main] Executable dir: %s", exeDir.c_str());
    LAppPal::PrintLog("[main] Model dir: %s", fullModelDir.c_str());
    LAppPal::PrintLog("[main] Window: %dx%d", cfg.GetWindow().width, cfg.GetWindow().height);

    if (!app.Initialize(fullModelDir, modelName, cfg.GetWindow().width, cfg.GetWindow().height)) {
        LAppPal::PrintLog("[main] FAILED to initialize application");
        MessageBoxW(nullptr, L"Failed to initialize Live2D Companion.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    LAppPal::PrintLog("[main] Application initialized successfully");

    if (cfg.GetWindow().always_on_top) {
        app.GetWindow()->SetAlwaysOnTop(true);
    }

    // Create the chat dialog window (Win32 fallback)
    g_chatDialog->Create(app.GetWindow()->GetHwnd(), GetModuleHandleW(nullptr));

    // Launch Python chat GUI (primary interface)
    LaunchChatGUI();

    {
        int x, y;
        app.GetWindow()->CalculateBottomRightPosition(x, y,
            cfg.GetWindow().margin_right,
            cfg.GetWindow().margin_bottom);
        app.GetWindow()->SetPosition(x, y);
    }

    app.GetWindow()->SetKeyCallback(HandleKeyPress);
    app.GetWindow()->SetMouseCallback(HandleMouseClick);

    std::thread ocrThread(OCRTimerLoop, cfg.GetOCR().interval_seconds);
    ocrThread.detach();

    LAppPal::PrintLog("[main] Entering main loop");

    auto lastTime = std::chrono::high_resolution_clock::now();
    bool greetingStarted = false;
    float greetingDelay = 2.0f;
    float elapsedSinceStart = 0.0f;

    int frameCount = 0;
    float fpsTimer = 0.0f;

    timeBeginPeriod(1);
    LARGE_INTEGER perfFreq;
    QueryPerformanceFrequency(&perfFreq);
    double targetFrameTime = 1.0 / 60.0;
    LARGE_INTEGER frameStartTime;
    QueryPerformanceCounter(&frameStartTime);

    app.GetView()->SetScale(1.3f);

    {
        LAppModel* m = app.GetModelManager()->GetModel(0);
        if (m && g_motionManager) {
            g_motionManager->SetModel(m);
        }
    }

    static int s_frameLogCounter = 0;
    while (g_running.load()) {
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                LAppPal::PrintLog("[main] Received WM_QUIT, exiting");
                g_running.store(false);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!g_running.load()) break;

        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        frameCount++;
        fpsTimer += deltaTime;

        s_frameLogCounter++;

        if (!greetingStarted) {
            elapsedSinceStart += deltaTime;
            if (elapsedSinceStart >= greetingDelay) {
                greetingStarted = true;
                LAppPal::PrintLog("[main] Auto-greeting triggered");
                g_chatDialog->Show();
                std::thread([]() {
                    DoChat("Hello master~ What's on screen? Keep me company!");
                }).detach();
            }
        }

        if (s_frameLogCounter <= 5) LAppPal::PrintLog("[main] Frame: motionManager update");
        if (g_motionManager) {
            g_motionManager->Update(deltaTime);
        }

        if (g_audioManager && g_audioManager->IsPlaying()) {
            LAppModel* model = app.GetModelManager()->GetModel(0);
            if (model && model->IsTalking()) {
                float amplitude = g_audioManager->GetCurrentAmplitude();
                model->UpdateLipSync(amplitude);
            }
        }

        if (s_frameLogCounter <= 5) LAppPal::PrintLog("[main] Frame: modelManager update");
        if (app.GetModelManager()) {
            app.GetModelManager()->Update();
        }

        if (s_frameLogCounter <= 5) LAppPal::PrintLog("[main] Frame: BeginFrame");
        app.GetWindow()->BeginFrame();

        if (s_frameLogCounter <= 5) LAppPal::PrintLog("[main] Frame: Render");
        app.Render();

        if (s_frameLogCounter <= 5) LAppPal::PrintLog("[main] Frame: EndFrame");
        app.GetWindow()->EndFrame();

        if (s_frameLogCounter <= 5) LAppPal::PrintLog("[main] Frame: complete");

        LARGE_INTEGER frameEndTime;
        QueryPerformanceCounter(&frameEndTime);
        double frameTime = static_cast<double>(frameEndTime.QuadPart - frameStartTime.QuadPart)
                         / perfFreq.QuadPart;
        if (frameTime < targetFrameTime) {
            double sleepTime = targetFrameTime - frameTime;
            DWORD sleepMs = static_cast<DWORD>(sleepTime * 1000.0);
            if (sleepMs > 0) {
                Sleep(sleepMs > 1 ? sleepMs - 1 : 0);
            }
            while (true) {
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                double elapsed = static_cast<double>(now.QuadPart - frameStartTime.QuadPart)
                               / perfFreq.QuadPart;
                if (elapsed >= targetFrameTime) break;
            }
        }
        QueryPerformanceCounter(&frameStartTime);
    }

    app.Release();
    g_appDelegate = nullptr;
    timeEndPeriod(1);

    if (g_chatDialog) g_chatDialog->Close();
    g_chatDialog.reset();
    g_expressionManager.reset();
    g_motionManager.reset();
    g_ocrManager.reset();
    g_ttsManager.reset();
    g_deepSeekClient.reset();
    g_audioManager.reset();

    return 0;
}
