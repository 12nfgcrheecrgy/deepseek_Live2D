#pragma once

#include <string>
#include <atomic>
#include <chrono>

class LAppModel;

class MotionManager {
public:
    enum class MotionType {
        Idle = 0,
        Talk,
        RaiseHand,
        Wave,
        Nod,
        ShakeHead,
        PointUp
    };

    enum class Priority {
        Force = 0,
        High = 1,
        Normal = 2,
        Idle = 3
    };

    MotionManager() = default;
    ~MotionManager() = default;

    void SetModel(LAppModel* model) { m_model = model; }

    bool StartMotion(MotionType type, Priority priority = Priority::Normal);
    void Update(float deltaTime);
    bool IsMotionPlaying() const;
    void StopMotion();
    MotionType GetCurrentMotion() const;

    const char* GetMotionName(MotionType type) const;

private:
    LAppModel* m_model = nullptr;
    MotionType m_currentMotion = MotionType::Idle;
    Priority m_currentPriority = Priority::Idle;
    std::atomic<bool> m_isPlaying{false};
    float m_elapsedTime = 0.0f;
    float m_motionDuration = 0.0f;
};