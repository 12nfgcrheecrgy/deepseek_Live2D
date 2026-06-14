#include "LAppView.h"
#include "LAppModel.h"
#include "LAppDelegate.h"
#include "CompanionWindow.h"
#include "LAppPal.h"
#include "Rendering/D3D11/CubismRenderer_D3D11.hpp"
#include <d3dcompiler.h>

static const char* g_spriteShaderCode = R"(
cbuffer ConstantBuffer : register(b0) {
    float4x4 projectMatrix;
    float4x4 clipMatrix;
    float4 baseColor;
    float4 channelFlag;
};
Texture2D mainTexture : register(t0);
SamplerState mainSampler : register(s0);
struct VS_IN { float2 pos : POSITION; float2 uv : TEXCOORD; };
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
VS_OUT VertNormal(VS_IN input) {
    VS_OUT output;
    output.pos = mul(float4(input.pos, 0.0f, 1.0f), projectMatrix);
    output.uv = float2(input.uv.x, 1.0f - input.uv.y);
    return output;
}
float4 PixelNormal(VS_OUT input) : SV_Target {
    return mainTexture.Sample(mainSampler, input.uv) * baseColor;
}
)";

static const int DefaultRenderTargetWidth = 1280;
static const int DefaultRenderTargetHeight = 720;

struct SpriteVertex {
    float x, y;
    float u, v;
};

LAppView::LAppView() {
}

LAppView::~LAppView() {
    Release();
}

void LAppView::Initialize() {
    int width = DefaultRenderTargetWidth;
    int height = DefaultRenderTargetHeight;

    m_deviceToScreen.LoadIdentity();
    if (width > 0 && height > 0) {
        float ratio = static_cast<float>(width) / static_cast<float>(height);
        float left = -ratio;
        float right = ratio;
        float bottom = -1.0f;
        float top = 1.0f;
        m_viewMatrix.SetScreenRect(left, right, bottom, top);
    }
    m_viewMatrix.SetMaxScale(2.0f);
    m_viewMatrix.SetMinScale(0.8f);
    m_viewMatrix.SetMaxScreenRect(-2.0f, 2.0f, -2.0f, 2.0f);

    InitializeSprite();
}

void LAppView::InitializeSprite() {
    LAppDelegate* delegate = nullptr;
    CompanionWindow* window = nullptr;
    ID3D11Device* device = nullptr;

    // We need LAppDelegate to get the D3D11 device; skip sprite setup if we can't get it
    // (in practice this won't happen since Initialize is called after delegate setup,
    // but LAppView doesn't hold a direct reference to LAppDelegate)
    Release();

    // For now, sprite initialization is deferred. The LAppView does not hold a reference
    // to the D3D11 device directly. In the current code flow, the sprite is never rendered,
    // so this is acceptable. The infrastructure is kept in case sprite usage is needed later.
}

void LAppView::Release() {
    if (m_spriteBlendState) { m_spriteBlendState->Release(); m_spriteBlendState = nullptr; }
    if (m_spriteSampler)    { m_spriteSampler->Release();    m_spriteSampler = nullptr; }
    if (m_spriteCB)         { m_spriteCB->Release();         m_spriteCB = nullptr; }
    if (m_spriteIB)         { m_spriteIB->Release();         m_spriteIB = nullptr; }
    if (m_spriteVB)         { m_spriteVB->Release();         m_spriteVB = nullptr; }
    if (m_spriteInputLayout){ m_spriteInputLayout->Release(); m_spriteInputLayout = nullptr; }
    if (m_spritePS)         { m_spritePS->Release();         m_spritePS = nullptr; }
    if (m_spriteVS)         { m_spriteVS->Release();         m_spriteVS = nullptr; }
}

void LAppView::Resize(int width, int height) {
    m_width = width;
    m_height = height;

    if (width <= 0 || height <= 0) return;

    float ratio = static_cast<float>(width) / static_cast<float>(height);
    float left = -ratio;
    float right = ratio;
    float bottom = -1.0f;
    float top = 1.0f;

    m_viewMatrix.SetScreenRect(left, right, bottom, top);

    float scaleW = static_cast<float>(width) / static_cast<float>(DefaultRenderTargetWidth);
    float scaleH = static_cast<float>(height) / static_cast<float>(DefaultRenderTargetHeight);
    float scale = (scaleW < scaleH) ? scaleW : scaleH;

    m_deviceToScreen.LoadIdentity();
    m_deviceToScreen.Scale(scale, scale);
}

void LAppView::Render(LAppModel* model) {
    if (!model) return;
    if (!model->GetModel()) return;

    int windowWidth = m_width;
    int windowHeight = m_height;

    if (windowWidth <= 0 || windowHeight <= 0) return;

    // Get D3D11 device context for the renderer
    extern LAppDelegate* g_appDelegate;
    ID3D11DeviceContext* d3dContext = nullptr;
    if (g_appDelegate && g_appDelegate->GetWindow()) {
        d3dContext = g_appDelegate->GetWindow()->GetD3dContext();
    }

    auto renderer = model->GetRenderer<Live2D::Cubism::Framework::Rendering::CubismRenderer_D3D11>();

    Live2D::Cubism::Framework::CubismMatrix44 projection;

    float canvasWidth = model->GetModel()->GetCanvasWidth();
    float canvasHeight = model->GetModel()->GetCanvasHeight();

    if (canvasWidth > 1.0f && windowWidth < windowHeight)
    {
        model->GetModelMatrix()->SetWidth(2.0f);
        projection.Scale(1.0f, static_cast<float>(windowWidth) / static_cast<float>(windowHeight));
    }
    else
    {
        projection.Scale(static_cast<float>(windowHeight) / static_cast<float>(windowWidth), 1.0f);
    }

    projection.MultiplyByMatrix(&m_viewMatrix);

    model->Update();

    if (renderer) {
        renderer->StartFrame(d3dContext);
        model->Draw(projection);
        renderer->EndFrame();
    }
}

void LAppView::RenderSprite() {
    // Sprite rendering is not currently used but infrastructure is kept.
}

float LAppView::TransformViewX(float deviceX) {
    int windowWidth = m_width > 0 ? m_width : 1;
    float screenX = (2.0f * deviceX / static_cast<float>(windowWidth)) - 1.0f;
    return m_deviceToScreen.TransformX(screenX);
}

float LAppView::TransformViewY(float deviceY) {
    int windowHeight = m_height > 0 ? m_height : 1;
    float screenY = (-2.0f * deviceY / static_cast<float>(windowHeight)) + 1.0f;
    return m_deviceToScreen.TransformY(screenY);
}

float LAppView::TransformScreenX(float viewX) {
    return m_deviceToScreen.InvertTransformX(viewX);
}

float LAppView::TransformScreenY(float viewY) {
    return m_deviceToScreen.InvertTransformY(viewY);
}
