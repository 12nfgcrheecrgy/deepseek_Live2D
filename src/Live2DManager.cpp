#include "Live2DManager.h"
#include "LAppModel.h"
#include "LAppPal.h"
#include "utils.h"

using namespace Csm;

Live2DManager::Live2DManager() {
}

Live2DManager::~Live2DManager() {
    Release();
}

bool Live2DManager::Initialize(const std::string& modelDir, const std::string& modelName) {
    m_modelDir = modelDir;
    m_modelName = modelName;

    LAppModel* model = new LAppModel();

    std::string model3JsonName = modelName + ".model3.json";

    if (!model->LoadAssets(modelDir, model3JsonName)) {
        LAppPal::PrintLog("[Live2DManager] Failed to load model assets from: %s/%s",
                          modelDir.c_str(), model3JsonName.c_str());
        delete model;
        return false;
    }

    m_models.PushBack(model);
    LAppPal::PrintLog("[Live2DManager] Model loaded successfully: %s", modelName.c_str());
    return true;
}

void Live2DManager::Release() {
    for (csmInt32 i = 0; i < m_models.GetSize(); i++) {
        delete m_models[i];
    }
    m_models.Clear();
}

void Live2DManager::Update() {
    LAppPal::UpdateTime();
    OnUpdate();
}

void Live2DManager::OnUpdate() {
    for (csmInt32 i = 0; i < m_models.GetSize(); i++) {
        if (m_models[i]) {
            m_models[i]->Update();
        }
    }
}

void Live2DManager::OnDrag(float x, float y) {
    for (csmInt32 i = 0; i < m_models.GetSize(); i++) {
        if (m_models[i]) {
            m_models[i]->OnDrag(x, y);
        }
    }
}

void Live2DManager::OnTap(float x, float y) {
    for (csmInt32 i = 0; i < m_models.GetSize(); i++) {
        if (m_models[i]) {
            m_models[i]->OnTap(x, y);
        }
    }
}

LAppModel* Live2DManager::GetModel(int index) const {
    if (index < 0 || index >= m_models.GetSize()) return nullptr;
    return m_models[index];
}

int Live2DManager::GetModelCount() const {
    return m_models.GetSize();
}

void Live2DManager::StartMotion(const char* group, int no, int priority) {
    for (csmInt32 i = 0; i < m_models.GetSize(); i++) {
        if (m_models[i]) {
            LAppPal::PrintLog("[Live2DManager] StartMotion: %s, %d, %d", group, no, priority);
            m_models[i]->StartMotion(group, no, priority);
            }
    }
}

void Live2DManager::StartRandomMotion(const char* group, int priority) {
    for (csmInt32 i = 0; i < m_models.GetSize(); i++) {
        if (m_models[i]) {
            m_models[i]->StartRandomMotion(group, priority);
        }
    }
}

void Live2DManager::SetExpression(const char* expressionId) {
    for (csmInt32 i = 0; i < m_models.GetSize(); i++) {
            LAppPal::PrintLog("[Live2DManager] Set expression: %s", expressionId);
        if (m_models[i]) {
            m_models[i]->SetExpression(expressionId);
        }
    }
}

void Live2DManager::SetExpressionByIndex(int index) {
    for (csmInt32 i = 0; i < m_models.GetSize(); i++) {
        if (m_models[i]) {
            m_models[i]->SetExpressionByIndex(index);
        }
    }
}