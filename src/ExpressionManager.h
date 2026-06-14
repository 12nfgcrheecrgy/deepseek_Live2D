#pragma once

#include <string>

namespace Live2D { namespace Cubism { namespace Framework { class CubismExpressionMotion; } } }

class ExpressionManager {
public:
    enum class ExpressionType {
        Neutral = 0,
        Happy,
        Surprised,
        Sad,
        Angry,
        Thinking
    };

    ExpressionManager() = default;
    ~ExpressionManager() = default;

    void SetExpression(ExpressionType type);
    ExpressionType GetCurrentExpression() const;
    ExpressionType MapToneToExpression(const std::string& tone) const;
    const char* GetExpressionName(ExpressionType type) const;

private:
    ExpressionType m_currentExpression = ExpressionType::Neutral;
};