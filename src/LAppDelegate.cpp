#include "LAppDelegate.h"
#include "CompanionWindow.h"
#include "LAppView.h"
#include "Live2DManager.h"
#include "LAppPal.h"
#include "CubismFramework.hpp"
#include "Rendering/D3D11/CubismRenderer_D3D11.hpp"
#include <Windows.h>
#include <cstdio>

LAppDelegate::LAppDelegate() {
}

LAppDelegate::~LAppDelegate() {
    Release();
}

bool LAppDelegate::Initialize(const std::string& modelDir, const std::string& modelName, int windowWidth, int windowHeight) {
    m_width = windowWidth;
    m_height = windowHeight;

    static std::string s_lastLogMessage;
    static int s_lastLogRepeatCount = 0;

    m_cubismOption.LogFunction = [](const char* message) {
        if (message && s_lastLogMessage == message) {
            s_lastLogRepeatCount++;
            return;
        }
        if (s_lastLogRepeatCount > 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), "(repeated %d times)", s_lastLogRepeatCount);
            LAppPal::PrintMessage(buf);
            s_lastLogRepeatCount = 0;
        }
        s_lastLogMessage = message ? message : "";
        LAppPal::PrintMessage(message);
    };
    m_cubismOption.LoggingLevel = Live2D::Cubism::Framework::CubismFramework::Option::LogLevel_Warning;
    m_cubismOption.LoadFileFunction = LAppPal::LoadFileBytes;
    m_cubismOption.ReleaseBytesFunction = LAppPal::ReleaseFileBytes;
    Live2D::Cubism::Framework::CubismFramework::StartUp(&m_allocator, &m_cubismOption);

    LAppPal::PrintLog("[LAppDelegate] CubismFramework StartUp returned: %d",
                      Live2D::Cubism::Framework::CubismFramework::IsStarted());

    Live2D::Cubism::Framework::CubismFramework::Initialize();

    m_window = std::make_unique<CompanionWindow>();
    if (!m_window->Create(m_width, m_height, "Live2D Companion")) {
        LAppPal::PrintLog("[LAppDelegate] Failed to create window");
        return false;
    }

    // Must be called before any renderer is created (i.e., before model loading)
    Live2D::Cubism::Framework::Rendering::CubismRenderer_D3D11::SetConstantSettings(1, m_window->GetD3dDevice());

    m_window->SetAlwaysOnTop(true);

    m_view = std::make_unique<LAppView>();
    m_view->Initialize();
    m_view->Resize(m_width, m_height);

    m_window->SetResizeCallback([this](int w, int h) {
        OnResize(w, h);
    });

    m_window->SetRenderCallback([this]() {
        Render();
    });

    m_modelManager = std::make_unique<Live2DManager>();
    LAppPal::PrintLog("[LAppDelegate] Loading model from: %s / %s", modelDir.c_str(), modelName.c_str());
    if (!m_modelManager->Initialize(modelDir, modelName)) {
        LAppPal::PrintLog("[LAppDelegate] Failed to initialize Live2D models");
        return false;
    }

    m_window->Show();
    LAppPal::PrintLog("[LAppDelegate] Window shown");

    LAppPal::PrintLog("[LAppDelegate] Initialized successfully");
    return true;
}

void LAppDelegate::Run() {
    m_running = true;
    LAppPal::PrintLog("[LAppDelegate] Entering main loop");

    MSG msg = {};
    while (m_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!m_running) break;

        if (PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE) == 0) {
            Sleep(1);
        }
    }
    LAppPal::PrintLog("[LAppDelegate] Main loop exited");
}

void LAppDelegate::Update() {
    if (m_modelManager) {
        m_modelManager->Update();
    }
}

void LAppDelegate::Render() {
    if (m_view && m_modelManager) {
        LAppModel* model = m_modelManager->GetModel(0);
        if (model) {
            m_view->Render(model);
        }
    }
}

void LAppDelegate::OnResize(int width, int height) {
    m_width = width;
    m_height = height;
    if (m_view) {
        m_view->Resize(width, height);
    }
}

void LAppDelegate::Release() {
    LAppPal::PrintLog("[LAppDelegate] Release called");
    m_running = false;

    if (m_modelManager) {
        m_modelManager->Release();
        m_modelManager.reset();
    }

    if (m_view) {
        m_view->Release();
        m_view.reset();
    }

    if (m_window) {
        m_window->Close();
        m_window.reset();
    }

    Live2D::Cubism::Framework::CubismFramework::Dispose();
}