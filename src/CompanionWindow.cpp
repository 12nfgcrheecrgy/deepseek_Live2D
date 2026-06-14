#include "CompanionWindow.h"
#include "utils.h"
#include "LAppPal.h"
#include "Rendering/D3D11/CubismDeviceInfo_D3D11.hpp"
#include <cstring>

CompanionWindow* g_currentWindowInstance = nullptr;

CompanionWindow::~CompanionWindow() {
    Close();
}

bool CompanionWindow::Create(int width, int height, const std::string& title) {
    m_width = width;
    m_height = height;

    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"Live2DCompanionWindow";

    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            LAppPal::PrintLog("[CompanionWindow] RegisterClassExW failed: %lu", err);
            return false;
        }
    }

    int x, y;
    CalculateBottomRightPosition(x, y);

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"Live2DCompanionWindow",
        utils::Utf8ToWide(title).c_str(),
        WS_POPUP,
        x, y,
        m_width, m_height,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!m_hwnd) {
        DWORD err = GetLastError();
        char buf[128];
        snprintf(buf, sizeof(buf), "[CompanionWindow] CreateWindowExW failed, error=%lu", err);
        LAppPal::PrintMessage(buf);
        return false;
    }

    g_currentWindowInstance = this;

    LAppPal::PrintLog("[CompanionWindow] Window created: %dx%d at (%d,%d)", m_width, m_height, x, y);

    if (!InitD3D11()) {
        LAppPal::PrintLog("[CompanionWindow] InitD3D11 failed");
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
        return false;
    }

    LAppPal::PrintLog("[CompanionWindow] D3D11 initialized successfully");
    return true;
}

void CompanionWindow::Show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
        m_visible = true;
    }
}

void CompanionWindow::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
        m_visible = false;
    }
}

void CompanionWindow::SetPosition(int x, int y) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }
}

void CompanionWindow::SetAlwaysOnTop(bool alwaysOnTop) {
    if (m_hwnd) {
        if (alwaysOnTop) {
            SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        } else {
            SetWindowPos(m_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
    }
}

void CompanionWindow::SetSize(int width, int height) {
    m_width = width;
    m_height = height;
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE);

        DestroyRenderTarget();

        if (m_swapChain) {
            HRESULT hr = m_swapChain->ResizeBuffers(1,
                static_cast<UINT>(width),
                static_cast<UINT>(height),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                0);
            if (SUCCEEDED(hr)) {
                CreateRenderTarget(static_cast<UINT>(width), static_cast<UINT>(height));
            }
        }

        if (m_resizeCallback) {
            m_resizeCallback(width, height);
        }
    }
}

void CompanionWindow::Close() {
    DestroyRenderTarget();

    if (m_depthState) {
        m_depthState->Release();
        m_depthState = nullptr;
    }

    if (m_swapChain) {
        m_swapChain->Release();
        m_swapChain = nullptr;
    }
    if (m_d3dContext) {
        m_d3dContext->Release();
        m_d3dContext = nullptr;
    }
    if (m_d3dDevice) {
        Csm::Rendering::CubismDeviceInfo_D3D11::ReleaseDeviceInfo(m_d3dDevice);
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
    }

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    g_currentWindowInstance = nullptr;
}

bool CompanionWindow::InitD3D11() {
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = static_cast<UINT>(m_width);
    scd.BufferDesc.Height = static_cast<UINT>(m_height);
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scd.OutputWindow = m_hwnd;
    scd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, &m_swapChain, &m_d3dDevice, &featureLevel, &m_d3dContext);
    if (FAILED(hr)) {
        LAppPal::PrintLog("[CompanionWindow] D3D11CreateDeviceAndSwapChain failed: 0x%08X", hr);
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = false;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthDesc.StencilEnable = false;
    hr = m_d3dDevice->CreateDepthStencilState(&depthDesc, &m_depthState);
    if (FAILED(hr)) {
        LAppPal::PrintLog("[CompanionWindow] CreateDepthStencilState failed: 0x%08X", hr);
        return false;
    }

    return CreateRenderTarget(static_cast<UINT>(m_width), static_cast<UINT>(m_height));
}

void CompanionWindow::CleanupD3D11() {
    DestroyRenderTarget();
    if (m_depthState) {
        m_depthState->Release();
        m_depthState = nullptr;
    }
    if (m_swapChain) {
        m_swapChain->Release();
        m_swapChain = nullptr;
    }
    if (m_d3dContext) {
        m_d3dContext->Release();
        m_d3dContext = nullptr;
    }
    if (m_d3dDevice) {
        Csm::Rendering::CubismDeviceInfo_D3D11::ReleaseDeviceInfo(m_d3dDevice);
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
    }
}

bool CompanionWindow::CreateRenderTarget(UINT width, UINT height) {
    if (!m_swapChain || !m_d3dDevice) return false;

    HRESULT hr;
    ID3D11Texture2D* backBuffer = nullptr;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(&backBuffer));
    if (FAILED(hr)) {
        LAppPal::PrintLog("[CompanionWindow] GetBuffer failed: 0x%08X", hr);
        return false;
    }
    hr = m_d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &m_renderTargetView);
    backBuffer->Release();
    if (FAILED(hr)) {
        LAppPal::PrintLog("[CompanionWindow] CreateRenderTargetView failed: 0x%08X", hr);
        return false;
    }

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;
    hr = m_d3dDevice->CreateTexture2D(&depthDesc, nullptr, &m_depthTexture);
    if (FAILED(hr)) {
        LAppPal::PrintLog("[CompanionWindow] CreateTexture2D (depth) failed: 0x%08X", hr);
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
    depthViewDesc.Format = depthDesc.Format;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthViewDesc.Texture2D.MipSlice = 0;
    hr = m_d3dDevice->CreateDepthStencilView(m_depthTexture, &depthViewDesc, &m_depthStencilView);
    if (FAILED(hr)) {
        LAppPal::PrintLog("[CompanionWindow] CreateDepthStencilView failed: 0x%08X", hr);
        return false;
    }

    return true;
}

void CompanionWindow::DestroyRenderTarget() {
    if (m_renderTargetView) {
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }
    if (m_depthStencilView) {
        m_depthStencilView->Release();
        m_depthStencilView = nullptr;
    }
    if (m_depthTexture) {
        m_depthTexture->Release();
        m_depthTexture = nullptr;
    }
}

void CompanionWindow::BeginFrame() {
    if (!m_d3dContext || !m_renderTargetView || !m_depthStencilView) return;

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_d3dContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
    m_d3dContext->ClearRenderTargetView(m_renderTargetView, clearColor);
    m_d3dContext->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    m_d3dContext->OMSetDepthStencilState(m_depthState, 0);
}

void CompanionWindow::EndFrame() {
    if (m_swapChain) {
        m_swapChain->Present(1, 0);
    }
}

void CompanionWindow::PollEvents() {
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void CompanionWindow::CalculateBottomRightPosition(int& outX, int& outY, int marginRight, int marginBottom) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    outX = screenWidth - m_width - marginRight;
    outY = screenHeight - m_height - marginBottom;
}

bool CompanionWindow::IsKeyDown(int vkey) const {
    return (GetAsyncKeyState(vkey) & 0x8000) != 0;
}

LRESULT CALLBACK CompanionWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CompanionWindow* window = g_currentWindowInstance;

    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            if (window) {
                window->m_width = LOWORD(lParam);
                window->m_height = HIWORD(lParam);
                if (window->m_resizeCallback) {
                    window->m_resizeCallback(window->m_width, window->m_height);
                }
            }
            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (window && window->m_keyCallback) {
                window->m_keyCallback(static_cast<int>(wParam), true);
            }
            return 0;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (window && window->m_keyCallback) {
                window->m_keyCallback(static_cast<int>(wParam), false);
            }
            return 0;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            if (window && window->m_mouseCallback) {
                int btn = (msg == WM_RBUTTONDOWN) ? 1 : 0;
                window->m_mouseCallback(btn, 1, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            if (window && window->m_mouseCallback) {
                int btn = (msg == WM_RBUTTONUP) ? 1 : 0;
                window->m_mouseCallback(btn, 0, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hwnd, msg, wParam, lParam);
            if (hit == HTCLIENT) {
                return HTCAPTION;
            }
            return hit;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
