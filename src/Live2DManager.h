#pragma once

#include <string>
#include <memory>
#include <vector>
#include "Type/csmVector.hpp"

class LAppModel;

class Live2DManager {
public:
    Live2DManager();
    ~Live2DManager();

    Live2DManager(const Live2DManager&) = delete;
    Live2DManager& operator=(const Live2DManager&) = delete;

    bool Initialize(const std::string& modelDir, const std::string& modelName);
    void Release();
    void Update();
    void OnDrag(float x, float y);
    void OnTap(float x, float y);
    void OnUpdate();

    LAppModel* GetModel(int index = 0) const;
    int GetModelCount() const;

    void StartMotion(const char* group, int no, int priority);
    void StartRandomMotion(const char* group, int priority);
    void SetExpression(const char* expressionId);
    void SetExpressionByIndex(int index);

private:
    Csm::csmVector<LAppModel*> m_models;
    std::string m_modelDir;
    std::string m_modelName;
};