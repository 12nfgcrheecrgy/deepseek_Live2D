#include "ExpressionManager.h"
#include <string>

void ExpressionManager::SetExpression(ExpressionType type) {
    m_currentExpression = type;
}

ExpressionManager::ExpressionType ExpressionManager::GetCurrentExpression() const {
    return m_currentExpression;
}

ExpressionManager::ExpressionType ExpressionManager::MapToneToExpression(const std::string& tone) const {
    if (tone == "happy") return ExpressionType::Happy;
    if (tone == "surprised") return ExpressionType::Surprised;
    if (tone == "sad") return ExpressionType::Sad;
    if (tone == "angry") return ExpressionType::Angry;
    if (tone == "thinking") return ExpressionType::Thinking;
    return ExpressionType::Neutral;
}

const char* ExpressionManager::GetExpressionName(ExpressionType type) const {
    switch (type) {
        case ExpressionType::Neutral:   return "Neutral";
        case ExpressionType::Happy:     return "Happy";
        case ExpressionType::Surprised:  return "Surprised";
        case ExpressionType::Sad:       return "Sad";
        case ExpressionType::Angry:     return "Angry";
        case ExpressionType::Thinking:  return "Thinking";
        default:                         return "Neutral";
    }
}