#pragma once

#include <string>
#include <memory>
#include "Live2DAllocator.h"
#include "CubismFramework.hpp"

class CompanionWindow;
class LAppView;
class Live2DManager;
class LAppModel;

class LAppDelegate {
public:
    LAppDelegate();
    ~LAppDelegate();

    LAppDelegate(const LAppDelegate&) = delete;
    LAppDelegate& operator=(const LAppDelegate&) = delete;

    bool Initialize(const std::string& modelDir, const std::string& modelName, int windowWidth, int windowHeight);
    void Run();
    void Release();

    CompanionWindow* GetWindow() const { return m_window.get(); }
    LAppView* GetView() const { return m_view.get(); }
    Live2DManager* GetModelManager() const { return m_modelManager.get(); }

    void Render();
    void Update();
    void OnResize(int width, int height);

private:

    std::unique_ptr<CompanionWindow> m_window;
    std::unique_ptr<LAppView> m_view;
    std::unique_ptr<Live2DManager> m_modelManager;

    Live2DAllocator m_allocator;
    Live2D::Cubism::Framework::CubismFramework::Option m_cubismOption;

    bool m_running = false;
    int m_width = 400;
    int m_height = 500;
};