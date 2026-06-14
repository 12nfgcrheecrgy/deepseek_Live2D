#pragma once

#include <string>
#include <memory>
#include <d3d11.h>
#include <DirectXMath.h>
#include "CubismFramework.hpp"
#include "Math/CubismMatrix44.hpp"
#include "Math/CubismViewMatrix.hpp"

class LAppModel;

class LAppView {
public:
    LAppView();
    ~LAppView();

    void Initialize();
    void Render(LAppModel* model);
    void InitializeSprite();
    void Release();
    void Resize(int width, int height);

    float TransformViewX(float deviceX);
    float TransformViewY(float deviceY);
    float TransformScreenX(float viewX);
    float TransformScreenY(float viewY);

    float GetScale() const { return m_scale; }
    void SetScale(float scale) { m_scale = scale; }
    void SetOffset(float x, float y) { m_offsetX = x; m_offsetY = y; }

private:
    void RenderSprite();

    Live2D::Cubism::Framework::CubismMatrix44 m_deviceToScreen;
    Live2D::Cubism::Framework::CubismViewMatrix m_viewMatrix;

    ID3D11VertexShader* m_spriteVS = nullptr;
    ID3D11PixelShader* m_spritePS = nullptr;
    ID3D11InputLayout* m_spriteInputLayout = nullptr;
    ID3D11Buffer* m_spriteVB = nullptr;
    ID3D11Buffer* m_spriteIB = nullptr;
    ID3D11Buffer* m_spriteCB = nullptr;
    ID3D11SamplerState* m_spriteSampler = nullptr;
    ID3D11BlendState* m_spriteBlendState = nullptr;

    int m_width = 0;
    int m_height = 0;
    float m_scale = 1.0f;
    float m_offsetX = 0.0f;
    float m_offsetY = 0.0f;
};
