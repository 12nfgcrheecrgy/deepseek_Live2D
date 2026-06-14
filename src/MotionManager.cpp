#include "MotionManager.h"
#include "LAppModel.h"
#include "LAppPal.h"

bool MotionManager::StartMotion(MotionType type, Priority priority) {
    LAppPal::PrintLog("[MotionManager] StartMotion: %s, priority=%d",
                      GetMotionName(type), static_cast<int>(priority));

    if (m_isPlaying.load() && static_cast<int>(priority) >= static_cast<int>(m_currentPriority)) {
        if (m_currentMotion != MotionType::Idle) {
            LAppPal::PrintLog("[MotionManager] Rejected: current=%s is playing with higher/equal priority",
                              GetMotionName(m_currentMotion));
            return false;
        }
    }

    m_currentMotion = type;
    m_currentPriority = priority;
    m_isPlaying.store(true);
    m_elapsedTime = 0.0f;

    switch (type) {
        case MotionType::Idle:       m_motionDuration = 0.0f;  break;
        case MotionType::Talk:       m_motionDuration = 3.0f;  break;
        case MotionType::RaiseHand:  m_motionDuration = 1.5f;  break;
        case MotionType::Wave:       m_motionDuration = 2.0f;  break;
        case MotionType::Nod:        m_motionDuration = 1.0f;  break;
        case MotionType::ShakeHead:  m_motionDuration = 1.0f;  break;
        case MotionType::PointUp:    m_motionDuration = 1.5f;  break;
        default:                     m_motionDuration = 1.0f;  break;
    }

    if (m_model) {
        switch (type) {
            case MotionType::Idle:
                m_model->SetAnimState(AnimState::Idle);
                break;
            case MotionType::Talk:
                m_model->SetAnimState(AnimState::Talking);
                break;
            case MotionType::RaiseHand:
                m_model->SetAnimState(AnimState::RaisingHand);
                break;
            case MotionType::Wave:
                m_model->SetAnimState(AnimState::Surprised);
                break;
            case MotionType::Nod:
                m_model->SetAnimState(AnimState::Happy);
                break;
            case MotionType::ShakeHead:
                m_model->SetAnimState(AnimState::Surprised);
                break;
            case MotionType::PointUp:
                m_model->SetAnimState(AnimState::Thinking);
                break;
            default:
                break;
        }
    }

    return true;
}

void MotionManager::Update(float deltaTime) {
    if (!m_isPlaying.load()) return;

    m_elapsedTime += deltaTime;

    if (m_motionDuration > 0.0f && m_elapsedTime >= m_motionDuration) {
        LAppPal::PrintLog("[MotionManager] Motion finished: %s", GetMotionName(m_currentMotion));
        m_isPlaying.store(false);
        m_currentMotion = MotionType::Idle;
        m_currentPriority = Priority::Idle;
        if (m_model) {
            m_model->SetAnimState(AnimState::Idle);
        }
    }
}

bool MotionManager::IsMotionPlaying() const {
    return m_isPlaying.load();
}

void MotionManager::StopMotion() {
    LAppPal::PrintLog("[MotionManager] StopMotion: %s", GetMotionName(m_currentMotion));
    m_isPlaying.store(false);
    m_currentMotion = MotionType::Idle;
    m_currentPriority = Priority::Idle;
    m_elapsedTime = 0.0f;
    if (m_model) {
        m_model->SetAnimState(AnimState::Idle);
    }
}

MotionManager::MotionType MotionManager::GetCurrentMotion() const {
    return m_currentMotion;
}

const char* MotionManager::GetMotionName(MotionType type) const {
    switch (type) {
        case MotionType::Idle:       return "Idle";
        case MotionType::Talk:       return "Talk";
        case MotionType::RaiseHand:  return "RaiseHand";
        case MotionType::Wave:       return "Wave";
        case MotionType::Nod:        return "Nod";
        case MotionType::ShakeHead:  return "ShakeHead";
        case MotionType::PointUp:    return "PointUp";
        default:                      return "Unknown";
    }
}
