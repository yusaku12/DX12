#pragma once

//!=======================================================
//! Dynamic Resolution 管理
//!=======================================================
class DynamicResolutionManager
{
public:

    static DynamicResolutionManager& Instance()
    {
        static DynamicResolutionManager instance;
        return instance;
    }

    void update();
    void renderDebugContents();

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:

    DynamicResolutionManager() = default;

    void applyScale(float scale);

    bool m_enabled = false;
    bool m_autoMode = false;

    float m_scale = 1.0f;
    float m_minScale = 0.67f;
    float m_maxScale = 1.0f;

    float m_targetFps = 60.0f;
    float m_stepDown = 0.03f;
    float m_stepUp = 0.02f;
    int m_cooldownFrames = 20;
    int m_cooldown = 0;
};