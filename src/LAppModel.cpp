#include "LAppModel.h"
#include "LAppDelegate.h"
#include "CompanionWindow.h"
#include "LAppPal.h"
#include "utils.h"
#include <Windows.h>
#include "Model/CubismMoc.hpp"
#include "Motion/CubismMotion.hpp"
#include "Motion/CubismMotionJson.hpp"
#include "Motion/CubismMotionQueueManager.hpp"
#include "Motion/CubismExpressionMotion.hpp"
#include "Motion/CubismExpressionMotionManager.hpp"
#include "Motion/CubismEyeBlinkUpdater.hpp"
#include "Effect/CubismBreath.hpp"
#include "Physics/CubismPhysics.hpp"
#include "Physics/CubismPhysicsJson.hpp"
#include "Utils/CubismJson.hpp"
#include "Utils/CubismString.hpp"
#include "Type/csmString.hpp"
#include "Id/CubismIdManager.hpp"
#include "CubismModelSettingJson.hpp"
#include "Rendering/D3D11/CubismRenderer_D3D11.hpp"
#include "Rendering/CubismRenderer.hpp"
#include <d3d11.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace Csm;

#include "stb/stb_image.h"

LAppModel::LAppModel()
    : CubismUserModel() {
}

LAppModel::~LAppModel() {
    ReleaseMotions();
    ReleaseExpressions();

    for (csmInt32 i = 0; i < m_textures.GetSize(); i++) {
        if (m_textures[i].textureView) {
            m_textures[i].textureView->Release();
            m_textures[i].textureView = nullptr;
        }
        if (m_textures[i].texture) {
            m_textures[i].texture->Release();
            m_textures[i].texture = nullptr;
        }
    }
    m_textures.Clear();

    if (m_modelSetting) {
        delete m_modelSetting;
        m_modelSetting = nullptr;
    }
}

bool LAppModel::LoadAssets(const std::string& dir, const std::string& fileName) {
    m_modelHomeDir = dir;
    m_modelName = fileName;

    std::string modelJsonPath = dir + "/" + fileName;
    LAppPal::LogDebug("[LAppModel] Loading model json: %s", modelJsonPath.c_str());

    if (!utils::FileExists(modelJsonPath)) {
        LAppPal::PrintLog("[LAppModel] Model json not found: %s", modelJsonPath.c_str());
        return false;
    }

    SetupModelSetting(modelJsonPath);

    std::string mocFileName = m_modelSetting->GetModelFileName();
    std::string mocPath = dir + "/" + std::string(mocFileName);

    std::vector<uint8_t> mocBytes = LAppPal::LoadFileAsBytes(mocPath);
    if (mocBytes.empty()) {
        LAppPal::PrintLog("[LAppModel] Failed to load moc: %s", mocPath.c_str());
        return false;
    }

    LoadModel(mocBytes.data(), static_cast<csmSizeInt>(mocBytes.size()));

    if (GetModel() == nullptr) {
        LAppPal::PrintLog("[LAppModel] ERROR: Model is null after LoadModel!");
        return false;
    }

    // Use actual window dimensions for render target creation
    int renderWidth = 400, renderHeight = 500;
    {
        extern LAppDelegate* g_appDelegate;
        if (g_appDelegate && g_appDelegate->GetWindow()) {
            renderWidth = g_appDelegate->GetWindow()->GetWidth();
            renderHeight = g_appDelegate->GetWindow()->GetHeight();
        }
    }
    CreateRenderer(renderWidth, renderHeight, 5);
    SetupTextures();

    _model->SaveParameters();

    for (csmInt32 i = 0; i < m_modelSetting->GetMotionGroupCount(); i++) {
        const char* group = m_modelSetting->GetMotionGroupName(i);
        PreloadMotionGroup(group);
    }

    if (_motionManager) {
        _motionManager->StopAllMotions();
    }

    _updating = true;
    _initialized = true;

    if (m_modelSetting->GetEyeBlinkParameterCount() > 0) {
        _eyeBlink = Live2D::Cubism::Framework::CubismEyeBlink::Create(m_modelSetting);
    }

    _breath = Live2D::Cubism::Framework::CubismBreath::Create();

    for (csmInt32 i = 0; i < m_modelSetting->GetExpressionCount(); i++) {
        const char* expName = m_modelSetting->GetExpressionName(i);
        std::string expFileName = m_modelSetting->GetExpressionFileName(i);
        std::string expPath = m_modelHomeDir + "/" + std::string(expFileName);
        if (utils::FileExists(expPath)) {
            std::vector<uint8_t> expData = LAppPal::LoadFileAsBytes(expPath);
            Live2D::Cubism::Framework::ACubismMotion* expMotion =
                LoadExpression(expData.data(), static_cast<csmSizeInt>(expData.size()), expName);
            if (expMotion && m_expressions.GetSize() < m_modelSetting->GetExpressionCount()) {
                m_expressions.PushBack(expMotion);
            }
        }
    }

    if (m_modelSetting->GetPhysicsFileName()) {
        std::string physicsPath = m_modelHomeDir + "/" + std::string(m_modelSetting->GetPhysicsFileName());
        if (utils::FileExists(physicsPath)) {
            std::vector<uint8_t> physicsData = LAppPal::LoadFileAsBytes(physicsPath);
            LoadPhysics(physicsData.data(), static_cast<csmSizeInt>(physicsData.size()));
        }
    }

    if (m_modelSetting->GetPoseFileName()) {
        std::string posePath = m_modelHomeDir + "/" + std::string(m_modelSetting->GetPoseFileName());
        if (utils::FileExists(posePath)) {
            std::vector<uint8_t> poseData = LAppPal::LoadFileAsBytes(posePath);
            LoadPose(poseData.data(), static_cast<csmSizeInt>(poseData.size()));
        }
    }

    m_idParamAngleX = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamAngleX");
    m_idParamAngleY = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamAngleY");
    m_idParamAngleZ = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamAngleZ");
    m_idParamBodyAngleX = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamBodyAngleX");
    m_idParamBodyAngleY = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamBodyAngleY");
    m_idParamBodyAngleZ = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamBodyAngleZ");
    m_idParamEyeBallX = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamEyeBallX");
    m_idParamEyeBallY = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamEyeBallY");
    m_idParamEyeLOpen = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamEyeLOpen");
    m_idParamEyeROpen = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamEyeROpen");
    m_idParamEyeLSmile = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamEyeLSmile");
    m_idParamEyeRSmile = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamEyeRSmile");
    m_idParamMouthOpenY = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamMouthOpenY");
    m_idParamMouthForm = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamMouthForm");
    m_idParamBreath = Live2D::Cubism::Framework::CubismFramework::GetIdManager()->GetId("ParamBreath");

    m_nextBlinkTime = 3.0f + static_cast<float>(rand()) / RAND_MAX * 4.0f;

    m_smoothBreath.Snap(0.0f);
    m_smoothBodySway.Snap(0.0f);
    m_smoothHeadTilt.Snap(0.0f);
    m_smoothHeadNod.Snap(0.0f);
    m_smoothEyeLSmile.Snap(0.0f);
    m_smoothEyeRSmile.Snap(0.0f);
    m_smoothMouthForm.Snap(0.0f);
    m_smoothMouthOpen.Snap(0.0f);
    m_smoothTappedAngleX.Snap(0.0f);

    LAppPal::PrintLog("[LAppModel] Model loaded: %s", m_modelName.c_str());
    return true;
}

void LAppModel::SetupModelSetting(const std::string& settingJsonPath) {
    std::vector<uint8_t> data = LAppPal::LoadFileAsBytes(settingJsonPath);
    m_modelSetting = new Live2D::Cubism::Framework::CubismModelSettingJson(data.data(), static_cast<csmSizeInt>(data.size()));
}

void LAppModel::SetupTextures() {
    for (csmInt32 i = 0; i < m_modelSetting->GetTextureCount(); i++) {
        std::string textureFileName = m_modelSetting->GetTextureFileName(i);
        std::string texturePath = m_modelHomeDir + "/" + textureFileName;
        int width = 0, height = 0;
        ID3D11ShaderResourceView* textureView = nullptr;
        ID3D11Texture2D* texture = nullptr;
        if (LoadTexture(texturePath, textureView, texture, width, height)) {
            TextureInfo info;
            info.textureView = textureView;
            info.texture = texture;
            info.width = width;
            info.height = height;
            m_textures.PushBack(info);

            auto* renderer = GetRenderer<Live2D::Cubism::Framework::Rendering::CubismRenderer_D3D11>();
            if (renderer) {
                renderer->BindTexture(i, textureView);
            }
        }
    }
}

bool LAppModel::LoadTexture(const std::string& filePath, ID3D11ShaderResourceView*& outTextureView, ID3D11Texture2D*& outTexture, int& outWidth, int& outHeight) {
    outWidth = 0; outHeight = 0;
    outTextureView = nullptr;
    outTexture = nullptr;

    std::wstring wPath = utils::Utf8ToWide(filePath);
    HANDLE hFile = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD fileSize = GetFileSize(hFile, nullptr);
    std::vector<unsigned char> buffer(fileSize);
    DWORD bytesRead = 0;
    ReadFile(hFile, buffer.data(), fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    int width, height, channels;
    unsigned char* data = stbi_load_from_memory(buffer.data(), static_cast<int>(bytesRead),
                                                &width, &height, &channels, 4);
    if (!data) {
        return false;
    }

    // D3D11 requires the LAppDelegate to access the device. Get it via the global app delegate.
    // In practice LoadAssets is called during Initialize which has the device ready.
    extern LAppDelegate* g_appDelegate;
    if (!g_appDelegate || !g_appDelegate->GetWindow()) {
        stbi_image_free(data);
        return false;
    }
    ID3D11Device* device = g_appDelegate->GetWindow()->GetD3dDevice();
    ID3D11DeviceContext* context = g_appDelegate->GetWindow()->GetD3dContext();
    if (!device || !context) {
        stbi_image_free(data);
        return false;
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = static_cast<UINT>(width);
    texDesc.Height = static_cast<UINT>(height);
    texDesc.MipLevels = 1;  // Provide only top-level mip; GenerateMips will create the rest
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;
    initData.SysMemPitch = static_cast<UINT>(width) * 4;
    initData.SysMemSlicePitch = 0;

    HRESULT hr = device->CreateTexture2D(&texDesc, &initData, &outTexture);
    if (FAILED(hr)) {
        stbi_image_free(data);
        LAppPal::PrintLog("[LAppModel] CreateTexture2D failed: 0x%08X", hr);
        return false;
    }

    hr = device->CreateShaderResourceView(outTexture, nullptr, &outTextureView);
    if (FAILED(hr)) {
        outTexture->Release();
        outTexture = nullptr;
        stbi_image_free(data);
        LAppPal::PrintLog("[LAppModel] CreateShaderResourceView failed: 0x%08X", hr);
        return false;
    }

    context->GenerateMips(outTextureView);

    stbi_image_free(data);
    outWidth = width;
    outHeight = height;
    return true;
}

void LAppModel::PreloadMotionGroup(const char* group) {
    csmInt32 motionCount = m_modelSetting->GetMotionCount(group);
    if (motionCount <= 0) return;

    csmVector<Live2D::Cubism::Framework::CubismMotion*> motionList;
    csmVector<Live2D::Cubism::Framework::CubismIdHandle> eyeBlinkIds;
    csmVector<Live2D::Cubism::Framework::CubismIdHandle> lipSyncIds;

    if (_eyeBlink) {
        eyeBlinkIds = _eyeBlink->GetParameterIds();
    }
    if (m_modelSetting->GetLipSyncParameterCount() > 0) {
        for (csmInt32 i = 0; i < m_modelSetting->GetLipSyncParameterCount(); i++) {
            lipSyncIds.PushBack(m_modelSetting->GetLipSyncParameterId(i));
        }
    }

    for (csmInt32 i = 0; i < motionCount; i++) {
        std::string motionFileName = m_modelSetting->GetMotionFileName(group, i);
        std::string motionPath = m_modelHomeDir + "/" + motionFileName;
        if (!utils::FileExists(motionPath)) continue;
        std::vector<uint8_t> motionData = LAppPal::LoadFileAsBytes(motionPath);
        if (motionData.empty()) continue;
        Live2D::Cubism::Framework::ACubismMotion* motion =
            LoadMotion(motionData.data(), static_cast<csmSizeInt>(motionData.size()), motionFileName.c_str());
        if (motion) {
            Live2D::Cubism::Framework::CubismMotion* cubismMotion =
                static_cast<Live2D::Cubism::Framework::CubismMotion*>(motion);
            cubismMotion->SetEffectIds(eyeBlinkIds, lipSyncIds);
            motionList.PushBack(cubismMotion);
        }
    }

    csmString key(group);
    m_motions[key] = motionList;
}

void LAppModel::ReleaseMotions() {
    for (auto it = m_motions.Begin(); it != m_motions.End(); ++it) {
        for (csmInt32 i = 0; i < it->Second.GetSize(); i++) {
            Live2D::Cubism::Framework::ACubismMotion::Delete(it->Second[i]);
        }
        it->Second.Clear();
    }
    m_motions.Clear();
}

void LAppModel::ReleaseExpressions() {
    for (csmInt32 i = 0; i < m_expressions.GetSize(); i++) {
        Live2D::Cubism::Framework::ACubismMotion::Delete(m_expressions[i]);
    }
    m_expressions.Clear();
}

void LAppModel::SetAnimState(AnimState state) {
    if (m_animState == state) return;
    m_prevAnimState = m_animState;
    m_animState = state;
    m_stateTime = 0.0f;
    m_stateTransitionTime = 0.0f;
}

void LAppModel::UpdateLipSync(float amplitude) {
    m_lipSyncAmplitude = amplitude;
    m_lipSyncActive = (amplitude > 0.01f);
}

void LAppModel::Update() {
    float dt = static_cast<float>(LAppPal::GetDeltaTime());
    if (dt > 0.1f) dt = 0.1f;

    m_customAnimTime += dt;
    m_stateTime += dt;
    if (m_stateTransitionTime < 0.35f) {
        m_stateTransitionTime += dt;
    }

    _dragManager->Update(dt);
    _model->LoadParameters();

    csmBool isMotionUpdated = false;
    if (_motionManager->IsFinished()) {
        if (_opacity) {
            StartRandomMotion("Idle", MotionPriorityIdle);
        }
    } else {
        isMotionUpdated = _motionManager->UpdateMotion(_model, dt);
    }

    _model->SaveParameters();

    if (!isMotionUpdated) {
        if (_eyeBlink) {
            _eyeBlink->UpdateParameters(_model, dt);
        }
    }

    if (_expressionManager) {
        _expressionManager->UpdateMotion(_model, dt);
    }

    csmFloat32 dragX = _dragManager->GetX();
    csmFloat32 dragY = _dragManager->GetY();

    _model->AddParameterValue(m_idParamAngleX, dragX * 30.0f);
    _model->AddParameterValue(m_idParamAngleY, dragY * 30.0f);
    _model->AddParameterValue(m_idParamAngleZ, dragX * dragY * -30.0f);
    _model->AddParameterValue(m_idParamBodyAngleX, dragX * 10.0f);
    _model->AddParameterValue(m_idParamEyeBallX, dragX);
    _model->AddParameterValue(m_idParamEyeBallY, dragY);

    float transitionT = m_stateTransitionTime / 0.35f;
    if (transitionT > 1.0f) transitionT = 1.0f;
    m_animIntensity = transitionT * transitionT * (3.0f - 2.0f * transitionT);

    switch (m_animState) {
        case AnimState::Idle:        UpdateIdleAnimation(dt);        break;
        case AnimState::Talking:     UpdateTalkingAnimation(dt);     break;
        case AnimState::Surprised:   UpdateSurprisedAnimation(dt);   break;
        case AnimState::Happy:       UpdateHappyAnimation(dt);       break;
        case AnimState::Thinking:    UpdateThinkingAnimation(dt);    break;
        case AnimState::Sleepy:      UpdateIdleAnimation(dt);        break;
        case AnimState::Tapped:      UpdateTappedAnimation(dt);      break;
        case AnimState::RaisingHand: UpdateRaisingHandAnimation(dt); break;
    }

    float breathTarget = sinf(m_customAnimTime * 1.8f) * 0.5f + 0.5f;
    float bodySwayTarget = sinf(m_customAnimTime * 2.0f) * 4.0f;
    float headTiltTarget = sinf(m_customAnimTime * 2.5f) * 3.0f;
    float headNodTarget = sinf(m_customAnimTime * 3.2f) * 2.0f;

    m_smoothBreath.SetTarget(breathTarget);
    m_smoothBodySway.SetTarget(bodySwayTarget);
    m_smoothHeadTilt.SetTarget(headTiltTarget);
    m_smoothHeadNod.SetTarget(headNodTarget);

    float breathVal = m_smoothBreath.Update(0.3f, dt);
    float bodySwayVal = m_smoothBodySway.Update(0.3f, dt);
    float headTiltVal = m_smoothHeadTilt.Update(0.3f, dt);
    float headNodVal = m_smoothHeadNod.Update(0.3f, dt);

    if (m_idParamBreath)     _model->AddParameterValue(m_idParamBreath, breathVal);
    if (m_idParamBodyAngleX) _model->AddParameterValue(m_idParamBodyAngleX, bodySwayVal);
    if (m_idParamAngleZ)     _model->AddParameterValue(m_idParamAngleZ, headTiltVal);
    if (m_idParamAngleX)     _model->AddParameterValue(m_idParamAngleX, headNodVal);

    float eyeSmileL = m_smoothEyeLSmile.Update(0.25f, dt);
    float eyeSmileR = m_smoothEyeRSmile.Update(0.25f, dt);
    float mouthForm = m_smoothMouthForm.Update(0.2f, dt);
    float mouthOpenSmooth = m_smoothMouthOpen.Update(0.15f, dt);

    if (m_idParamEyeLSmile) _model->AddParameterValue(m_idParamEyeLSmile, eyeSmileL);
    if (m_idParamEyeRSmile) _model->AddParameterValue(m_idParamEyeRSmile, eyeSmileR);
    if (m_idParamMouthForm) _model->AddParameterValue(m_idParamMouthForm, mouthForm);
    if (m_idParamMouthOpenY && !m_lipSyncActive && !IsTalking()) {
        _model->AddParameterValue(m_idParamMouthOpenY, mouthOpenSmooth - 0.5f);
    }

    if (m_lipSyncActive) {
        m_lipSyncSmoothAmplitude += (m_lipSyncAmplitude - m_lipSyncSmoothAmplitude) * dt * 8.0f;
        float mouthVal = m_lipSyncSmoothAmplitude * 3.0f;
        if (mouthVal > 1.0f) mouthVal = 1.0f;
        if (m_idParamMouthOpenY) _model->AddParameterValue(m_idParamMouthOpenY, mouthVal * 0.8f);

        float lipForm = sinf(m_customAnimTime * 14.0f) * m_lipSyncSmoothAmplitude * 0.15f;
        if (m_idParamMouthForm) _model->AddParameterValue(m_idParamMouthForm, lipForm);
    }

    UpdateBlinkSystem(dt);
    UpdateMicroExpressions(dt);

    if (_breath)  _breath->UpdateParameters(_model, dt);
    if (_physics) _physics->Evaluate(_model, dt);
    if (_pose)    _pose->UpdateParameters(_model, dt);

    _model->Update();
}

void LAppModel::UpdateIdleAnimation(float dt) {
    m_smoothEyeLSmile.SetTarget(0.0f);
    m_smoothEyeRSmile.SetTarget(0.0f);
    m_smoothMouthForm.SetTarget(0.0f);
    m_smoothMouthOpen.SetTarget(0.0f);
    m_lipSyncActive = false;
}

void LAppModel::UpdateTalkingAnimation(float dt) {
    float bodyLean = sinf(m_customAnimTime * 3.5f) * 4.0f;
    if (m_idParamBodyAngleX) {
        float currentSway = m_smoothBodySway.target;
        m_smoothBodySway.SetTarget(currentSway + bodyLean * 0.5f);
    }

    float headShake = sinf(m_customAnimTime * 3.8f) * 2.0f + sinf(m_customAnimTime * 7.0f) * 1.0f;
    if (m_idParamAngleX) {
        float currentNod = m_smoothHeadNod.target;
        m_smoothHeadNod.SetTarget(currentNod + headShake * 0.5f);
    }

    m_smoothEyeLSmile.SetTarget(0.35f);
    m_smoothEyeRSmile.SetTarget(0.35f);

    if (!m_lipSyncActive) {
        float mouthOpen = fabsf(sinf(m_customAnimTime * 8.0f)) * 0.7f;
        float mouthForm = sinf(m_customAnimTime * 12.0f) * 0.2f;
        if (m_idParamMouthOpenY) _model->AddParameterValue(m_idParamMouthOpenY, mouthOpen);
        if (m_idParamMouthForm) _model->AddParameterValue(m_idParamMouthForm, mouthForm);
    }
}

void LAppModel::UpdateHappyAnimation(float dt) {
    m_smoothEyeLSmile.SetTarget(0.55f);
    m_smoothEyeRSmile.SetTarget(0.55f);
    m_smoothMouthForm.SetTarget(0.4f);

    float bounce = fabsf(sinf(m_customAnimTime * 4.0f)) * 4.0f;
    if (m_idParamBodyAngleX) m_smoothBodySway.SetTarget(m_smoothBodySway.target + bounce * 0.3f);
}

void LAppModel::UpdateSurprisedAnimation(float dt) {
    if (m_idParamEyeLOpen) _model->AddParameterValue(m_idParamEyeLOpen, 0.5f);
    if (m_idParamEyeROpen) _model->AddParameterValue(m_idParamEyeROpen, 0.5f);
    m_smoothMouthForm.SetTarget(-0.2f);
    m_smoothMouthOpen.SetTarget(0.6f);

    float leanBack = 6.0f;
    if (m_idParamBodyAngleX) m_smoothBodySway.SetTarget(m_smoothBodySway.target + leanBack * 0.4f);
}

void LAppModel::UpdateThinkingAnimation(float dt) {
    m_smoothEyeLSmile.SetTarget(-0.05f);
    m_smoothEyeRSmile.SetTarget(-0.05f);

    if (m_idParamEyeLOpen) _model->AddParameterValue(m_idParamEyeLOpen, -0.1f);
    if (m_idParamEyeROpen) _model->AddParameterValue(m_idParamEyeROpen, -0.1f);
}

void LAppModel::UpdateTappedAnimation(float dt) {
    m_tapAnimTimer += dt;
    float t = m_tapAnimTimer / m_tapAnimDuration;
    if (t > 1.0f) t = 1.0f;

    float tapAngle = 0.0f;
    if (t < 0.3f) {
        tapAngle = (t / 0.3f) * -10.0f;
    } else if (t < 0.6f) {
        tapAngle = ((t - 0.3f) / 0.3f) * 16.0f - 10.0f;
    } else {
        tapAngle = (1.0f - (t - 0.6f) / 0.4f) * 6.0f;
    }

    m_smoothTappedAngleX.SetTarget(tapAngle);
    float angleVal = m_smoothTappedAngleX.Update(0.05f, dt);
    if (m_idParamAngleX) _model->AddParameterValue(m_idParamAngleX, angleVal);

    if (t >= 1.0f) {
        m_tapAnimTimer = 0.0f;
        SetAnimState(AnimState::Idle);
    }
}

void LAppModel::UpdateRaisingHandAnimation(float dt) {
    m_raisingHandTimer += dt;
    float t = m_raisingHandTimer / m_raisingHandDuration;
    if (t > 1.0f) t = 1.0f;

    float wavePhase = sinf(m_raisingHandTimer * 14.0f) * (t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f + t);

    if (m_idParamBodyAngleX) _model->AddParameterValue(m_idParamBodyAngleX, wavePhase * 2.5f);
    if (m_idParamAngleZ)     _model->AddParameterValue(m_idParamAngleZ, wavePhase * 1.5f);
    if (m_idParamAngleY)     _model->AddParameterValue(m_idParamAngleY, wavePhase * 1.0f);

    if (t >= 1.0f) {
        m_raisingHandTimer = 0.0f;
        SetAnimState(AnimState::Idle);
    }
}

void LAppModel::UpdateBlinkSystem(float dt) {
    if (!m_autoBlinkEnabled) return;

    m_blinkTimer += dt;

    if (m_blinkTimer >= m_nextBlinkTime) {
        if (!m_doubleBlink) {
            m_doubleBlink = ((float)rand() / RAND_MAX) < 0.15f;
            m_doubleBlinkCount = m_doubleBlink ? 2 : 1;
            m_blinkPhase = 0.0f;
        }
        m_blinkTimer = 0.0f;
    }

    float blinkValue = 1.0f;
    float closeTime = 0.08f;
    float openTime = 0.12f;
    float totalBlink = closeTime + openTime;

    if (m_blinkPhase < closeTime) {
        blinkValue = 0.0f;
    } else if (m_blinkPhase < totalBlink) {
        blinkValue = (m_blinkPhase - closeTime) / openTime;
    }

    m_blinkPhase += dt;

    if (m_blinkPhase >= totalBlink * m_doubleBlinkCount) {
        m_blinkPhase = 0.0f;
        m_blinkTimer = 0.0f;
        m_doubleBlink = false;
        m_nextBlinkTime = 3.0f + static_cast<float>(rand()) / RAND_MAX * 4.0f;
    }

    if (blinkValue < 0.95f) {
        float squint = (1.0f - blinkValue) * 0.3f;
        if (m_idParamEyeLSmile) _model->AddParameterValue(m_idParamEyeLSmile, squint);
        if (m_idParamEyeRSmile) _model->AddParameterValue(m_idParamEyeRSmile, squint);
    }

    if (m_idParamEyeLOpen) _model->AddParameterValue(m_idParamEyeLOpen, blinkValue);
    if (m_idParamEyeROpen) _model->AddParameterValue(m_idParamEyeROpen, blinkValue);
}

void LAppModel::UpdateMicroExpressions(float dt) {
    m_microExpressionTimer += dt;

    if (m_microExpressionTimer >= m_nextMicroExpressionTime) {
        m_currentMicroExpression = rand() % 4;
        m_microExpressionTimer = 0.0f;
        m_nextMicroExpressionTime = 3.0f + static_cast<float>(rand()) / RAND_MAX * 5.0f;
    }

    float intensity = 0.0f;
    float microDuration = 1.0f;
    if (m_animState == AnimState::Idle || m_animState == AnimState::Thinking) {
        intensity = sinf(m_microExpressionTimer / microDuration * 3.14159f) * 0.12f;
    }

    if (intensity > 0.001f) {
        switch (m_currentMicroExpression) {
            case 0:
                if (m_idParamEyeLSmile) _model->AddParameterValue(m_idParamEyeLSmile, intensity);
                if (m_idParamEyeRSmile) _model->AddParameterValue(m_idParamEyeRSmile, intensity);
                break;
            case 1:
                if (m_idParamAngleZ) _model->AddParameterValue(m_idParamAngleZ, intensity * 3.0f);
                break;
            case 2:
                if (m_idParamMouthForm) _model->AddParameterValue(m_idParamMouthForm, intensity * 1.5f);
                break;
            case 3:
                if (m_idParamAngleX) _model->AddParameterValue(m_idParamAngleX, intensity * 2.0f);
                break;
        }
    }
}

void LAppModel::Draw(Live2D::Cubism::Framework::CubismMatrix44& matrix) {
    if (!_model) {
        LAppPal::PrintLog("[LAppModel::Draw] _model is null");
        return;
    }

    auto renderer = GetRenderer<Live2D::Cubism::Framework::Rendering::CubismRenderer_D3D11>();
    if (!renderer) {
        LAppPal::PrintLog("[LAppModel::Draw] renderer is null");
        return;
    }

    matrix.MultiplyByMatrix(_modelMatrix);
    renderer->SetMvpMatrix(&matrix);

    renderer->DrawModel();
}

bool LAppModel::StartMotion(const char* group, int no, int priority,
                             Live2D::Cubism::Framework::ACubismMotion::FinishedMotionCallback onFinished) {
    if (!_motionManager) {
        LAppPal::PrintLog("[LAppModel] StartMotion failed: _motionManager is null");
        return false;
    }

    csmInt32 motionCount = m_modelSetting->GetMotionCount(group);
    if (no < 0 || no >= motionCount) {
        LAppPal::PrintLog("[LAppModel] StartMotion failed: motion no %d out of range [0, %d) for group '%s'",
                          no, motionCount, group);
        return false;
    }

    csmString key(group);
    if (m_motions.IsExist(key) && m_motions[key].GetSize() > no && m_motions[key][no]) {
        LAppPal::PrintLog("[LAppModel] StartMotion preloaded motion available: group='%s', no=%d", group, no);
    }

    std::string motionFileName = m_modelSetting->GetMotionFileName(group, no);
    std::string motionPath = m_modelHomeDir + "/" + motionFileName;
    if (!utils::FileExists(motionPath)) {
        LAppPal::PrintLog("[LAppModel] StartMotion failed: motion file not found: %s", motionPath.c_str());
        return false;
    }
    std::vector<uint8_t> motionData = LAppPal::LoadFileAsBytes(motionPath);
    if (motionData.empty()) {
        LAppPal::PrintLog("[LAppModel] StartMotion failed: empty motion data: %s", motionPath.c_str());
        return false;
    }
    Live2D::Cubism::Framework::ACubismMotion* motion =
        LoadMotion(motionData.data(), static_cast<csmSizeInt>(motionData.size()), motionFileName.c_str(), onFinished);
    if (!motion) {
        LAppPal::PrintLog("[LAppModel] StartMotion failed: LoadMotion returned null for %s", motionPath.c_str());
        return false;
    }
    LAppPal::PrintLog("[LAppModel] StartMotion loaded: group='%s', no=%d, file=%s", group, no, motionFileName.c_str());

    csmVector<Live2D::Cubism::Framework::CubismIdHandle> eyeBlinkIds;
    csmVector<Live2D::Cubism::Framework::CubismIdHandle> lipSyncIds;
    if (_eyeBlink) eyeBlinkIds = _eyeBlink->GetParameterIds();
    if (m_modelSetting->GetLipSyncParameterCount() > 0) {
        for (csmInt32 i = 0; i < m_modelSetting->GetLipSyncParameterCount(); i++) {
            lipSyncIds.PushBack(m_modelSetting->GetLipSyncParameterId(i));
        }
    }
    Live2D::Cubism::Framework::CubismMotion* cubismMotion =
        static_cast<Live2D::Cubism::Framework::CubismMotion*>(motion);
    cubismMotion->SetEffectIds(eyeBlinkIds, lipSyncIds);

    bool result = _motionManager->StartMotionPriority(motion, false, priority);
    LAppPal::PrintLog("[LAppModel] StartMotion result: %s (group='%s', no=%d, priority=%d)",
                      result ? "SUCCESS" : "FAILED", group, no, priority);
    return result;
}

bool LAppModel::StartRandomMotion(const char* group, int priority,
                                   Live2D::Cubism::Framework::ACubismMotion::FinishedMotionCallback onFinished) {
    csmInt32 motionCount = m_modelSetting->GetMotionCount(group);
    if (motionCount <= 0) {
        LAppPal::PrintLog("[LAppModel] StartRandomMotion failed: no motions in group '%s'", group);
        return false;
    }
    csmInt32 no = utils::GetRandomInt(0, motionCount - 1);
    LAppPal::PrintLog("[LAppModel] StartRandomMotion: group='%s', no=%d/%d, priority=%d", group, no, motionCount, priority);
    return StartMotion(group, no, priority, onFinished);
}

void LAppModel::SetExpression(const char* expressionId) {
    if (!expressionId || !_expressionManager) return;
    for (csmInt32 i = 0; i < m_expressions.GetSize(); i++) {
        const char* expName = m_modelSetting->GetExpressionName(i);
        if (expName && strcmp(expName, expressionId) == 0 && m_expressions[i]) {
            _expressionManager->StartMotion(m_expressions[i], false);
            return;
        }
    }
}

void LAppModel::SetExpressionByIndex(int index) {
    if (index < 0 || index >= m_expressions.GetSize()) return;
    if (m_expressions[index] && _expressionManager) {
        _expressionManager->StartMotion(m_expressions[index], false);
    }
}

void LAppModel::OnDrag(float x, float y) {
    _dragManager->Set(x, y);
}

void LAppModel::OnTap(float x, float y) {
    if (_opacity) {
        SetAnimState(AnimState::Tapped);
        StartRandomMotion("TapBody", MotionPriorityNormal);
    }
}

int LAppModel::GetExpressionCount() const {
    return m_expressions.GetSize();
}

const char* LAppModel::GetExpressionName(int index) const {
    if (index < 0 || index >= m_modelSetting->GetExpressionCount()) return "";
    return m_modelSetting->GetExpressionName(index);
}

void LAppModel::SetAutoBlinkEnable(bool enable) {
    m_autoBlinkEnabled = enable;
    if (!enable) {
        m_smoothEyeLSmile.SetTarget(0.0f);
        m_smoothEyeRSmile.SetTarget(0.0f);
    }
}

void LAppModel::SetAutoBreathEnable(bool enable) {
    m_autoBreathEnabled = enable;
}