#pragma once

#include <string>
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <functional>
#include <vector>

class CompanionWindow {
public:
    CompanionWindow() = default;
    ~CompanionWindow();

    CompanionWindow(const CompanionWindow&) = delete;
    CompanionWindow& operator=(const CompanionWindow&) = delete;
    
    bool Create(int width, int height, const std::string& title);
    void Show();
    void Hide();
    void SetPosition(int x, int y);
    void SetAlwaysOnTop(bool alwaysOnTop);
    void SetSize(int width, int height);
    void Close();

    HWND GetHwnd() const { return m_hwnd; }
    ID3D11Device* GetD3dDevice() const { return m_d3dDevice; }
    ID3D11DeviceContext* GetD3dContext() const { return m_d3dContext; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool IsVisible() const { return m_visible; }

    void SetRenderCallback(std::function<void()> callback) { m_renderCallback = callback; }
    void SetResizeCallback(std::function<void(int, int)> callback) { m_resizeCallback = callback; }
    void SetKeyCallback(std::function<void(int, bool)> callback) { m_keyCallback = callback; }
    void SetMouseCallback(std::function<void(int, int, int, int)> callback) { m_mouseCallback = callback; }

    void PollEvents();

    void CalculateBottomRightPosition(int& outX, int& outY, int marginRight = 20, int marginBottom = 20);

    bool IsKeyDown(int vkey) const;

    void BeginFrame();
    void EndFrame();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool InitD3D11();
    void CleanupD3D11();
    bool CreateRenderTarget(UINT width, UINT height);
    void DestroyRenderTarget();

    HWND m_hwnd = nullptr;
    int m_width = 400;
    int m_height = 500;
    bool m_visible = false;

    ID3D11Device* m_d3dDevice = nullptr;
    ID3D11DeviceContext* m_d3dContext = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11RenderTargetView* m_renderTargetView = nullptr;
    ID3D11Texture2D* m_depthTexture = nullptr;
    ID3D11DepthStencilView* m_depthStencilView = nullptr;
    ID3D11DepthStencilState* m_depthState = nullptr;

    std::function<void()> m_renderCallback;
    std::function<void(int, int)> m_resizeCallback;
    std::function<void(int, bool)> m_keyCallback;
    std::function<void(int, int, int, int)> m_mouseCallback;
};

extern CompanionWindow* g_currentWindowInstance;
