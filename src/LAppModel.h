#pragma once

#include <string>
#include <memory>
#include <vector>
#include <d3d11.h>
#include "Model/CubismUserModel.hpp"
#include "CubismFramework.hpp"
#include "ICubismModelSetting.hpp"
#include "Type/csmVector.hpp"
#include "Id/CubismId.hpp"
#include "Motion/ACubismMotion.hpp"
#include "Motion/CubismMotion.hpp"
#include "Motion/CubismExpressionMotion.hpp"

static const int MotionPriorityIdle = 1;
static const int MotionPriorityNormal = 3;

enum class AnimState { Idle, Talking, Happy, Surprised, Thinking, Sleepy, Tapped, RaisingHand };

struct TextureInfo {
    ID3D11ShaderResourceView* textureView = nullptr;
    ID3D11Texture2D* texture = nullptr;
    int width = 0;
    int height = 0;
};

struct SmoothParam {
    float target = 0.0f;
    float current = 0.0f;
    float velocity = 0.0f;
    void SetTarget(float t) { target = t; }
    float Update(float smoothTime, float dt) {
        float omega = 2.0f / (smoothTime + 0.0001f);
        float x = omega * dt;
        float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        float change = current - target;
        float temp = (velocity + omega * change) * dt;
        velocity = (velocity - omega * temp) * exp;
        current = target + (change + temp) * exp;
        if (fabsf(current - target) < 0.0001f) { current = target; velocity = 0.0f; }
        return current;
    }
    void Snap(float value) { current = value; target = value; velocity = 0.0f; }
};

class LAppModel : public Live2D::Cubism::Framework::CubismUserModel {
public:
    LAppModel();
    ~LAppModel() override;

    LAppModel(const LAppModel&) = delete;
    LAppModel& operator=(const LAppModel&) = delete;

    bool LoadAssets(const std::string& dir, const std::string& fileName);
    void Update();
    void UpdateLipSync(float amplitude);
    void Draw(Live2D::Cubism::Framework::CubismMatrix44& matrix);

    bool StartMotion(const char* group, int no, int priority, Live2D::Cubism::Framework::ACubismMotion::FinishedMotionCallback onFinished = nullptr);
    bool StartRandomMotion(const char* group, int priority, Live2D::Cubism::Framework::ACubismMotion::FinishedMotionCallback onFinished = nullptr);
    void SetExpression(const char* expressionId);
    void SetExpressionByIndex(int index);

    void OnDrag(float x, float y);
    void OnTap(float x, float y);

    int GetExpressionCount() const;
    const char* GetExpressionName(int index) const;

    void SetAutoBlinkEnable(bool enable);
    void SetAutoBreathEnable(bool enable);

    void SetAnimState(AnimState state);
    AnimState GetAnimState() const { return m_animState; }
    bool IsTalking() const { return m_animState == AnimState::Talking; }

private:
    void SetupModelSetting(const std::string& settingJsonPath);
    void SetupTextures();
    void PreloadMotionGroup(const char* group);
    void ReleaseMotions();
    void ReleaseExpressions();

    void UpdateIdleAnimation(float dt);
    void UpdateTalkingAnimation(float dt);
    void UpdateSurprisedAnimation(float dt);
    void UpdateHappyAnimation(float dt);
    void UpdateThinkingAnimation(float dt);
    void UpdateTappedAnimation(float dt);
    void UpdateRaisingHandAnimation(float dt);
    void UpdateBlinkSystem(float dt);
    void UpdateMicroExpressions(float dt);

    bool LoadTexture(const std::string& filePath, ID3D11ShaderResourceView*& outTextureView, ID3D11Texture2D*& outTexture, int& outWidth, int& outHeight);

    Live2D::Cubism::Framework::ICubismModelSetting* m_modelSetting = nullptr;
    std::string m_modelHomeDir;
    std::string m_modelName;

    Csm::csmVector<TextureInfo> m_textures;

    Csm::csmMap<Csm::csmString, Csm::csmVector<Live2D::Cubism::Framework::CubismMotion*>> m_motions;
    Csm::csmVector<Live2D::Cubism::Framework::ACubismMotion*> m_expressions;

    Live2D::Cubism::Framework::CubismMotionQueueEntry* m_motionQueueEntry = nullptr;

    const Live2D::Cubism::Framework::CubismId* m_idParamAngleX = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamAngleY = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamAngleZ = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamBodyAngleX = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamBodyAngleY = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamBodyAngleZ = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamEyeBallX = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamEyeBallY = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamEyeLOpen = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamEyeROpen = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamEyeLSmile = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamEyeRSmile = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamMouthOpenY = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamMouthForm = nullptr;
    const Live2D::Cubism::Framework::CubismId* m_idParamBreath = nullptr;

    bool m_autoBlinkEnabled = true;
    bool m_autoBreathEnabled = true;

    float m_customAnimTime = 0.0f;
    float m_blinkTimer = 0.0f;
    float m_blinkPhase = 0.0f;
    float m_nextBlinkTime = 3.0f;
    bool m_doubleBlink = false;
    int m_doubleBlinkCount = 0;

    AnimState m_animState = AnimState::Idle;
    AnimState m_prevAnimState = AnimState::Idle;
    float m_stateTime = 0.0f;
    float m_stateTransitionTime = 0.0f;
    float m_animIntensity = 1.0f;

    SmoothParam m_smoothBreath;
    SmoothParam m_smoothBodySway;
    SmoothParam m_smoothHeadTilt;
    SmoothParam m_smoothHeadNod;
    SmoothParam m_smoothEyeLSmile;
    SmoothParam m_smoothEyeRSmile;
    SmoothParam m_smoothMouthForm;
    SmoothParam m_smoothMouthOpen;
    SmoothParam m_smoothTappedAngleX;

    float m_microExpressionTimer = 0.0f;
    float m_nextMicroExpressionTime = 5.0f;
    int m_currentMicroExpression = 0;

    float m_lipSyncAmplitude = 0.0f;
    float m_lipSyncSmoothAmplitude = 0.0f;
    bool m_lipSyncActive = false;

    float m_tapAnimTimer = 0.0f;
    float m_tapAnimDuration = 0.5f;

    float m_raisingHandTimer = 0.0f;
    float m_raisingHandDuration = 1.5f;

    float m_scaleFactor = 1.0f;
};