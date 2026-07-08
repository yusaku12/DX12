#pragma once

#include "Component.h"

class AnimationComponent;

//=====================================================
//! Unity Timeline 風の簡易タイムライン
//! - AnimationComponent のクリップ再生を時刻ベースで制御
//! - シグナルイベントを EventBus へ発行
//! - Inspector 上でシーケンサー編集
//=====================================================
class TimelineComponent : public Component
{
public:

    enum class BlendCurve : int32_t
    {
        Linear = 0,
        EaseIn = 1,
        EaseOut = 2,
        EaseInOut = 3,
        SmoothStep = 4,
    };

    struct Clip
    {
        bool enabled = true;
        std::string animationName;
        int track = 0;
        float weight = 1.0f;
        float startTime = 0.0f;
        float duration = 1.0f;
        float clipInTime = 0.0f;
        float speed = 1.0f;
        bool loop = true;
        float blendIn = 0.1f;
        float blendOut = 0.1f;
        BlendCurve blendInCurve = BlendCurve::EaseOut;
        BlendCurve blendOutCurve = BlendCurve::EaseIn;
    };

    struct Signal
    {
        bool enabled = true;
        std::string eventName;
        float time = 0.0f;
    };

    TimelineComponent() = default;
    ~TimelineComponent() override = default;

    void awake() override;
    void update() override;
    void onDisable() override;
    void onDestroy() override;
    void inspectGUI() override;

    void play();
    void pause();
    void resume();
    void stop(bool resetTime = true);

    bool isPlaying() const { return m_playing && !m_paused; }
    bool isPaused() const { return m_paused; }

    void setCurrentTime(float seconds);
    float getCurrentTime() const { return m_currentTime; }

    void setDuration(float seconds) { m_duration = std::max(seconds, 0.05f); }
    float getDuration() const { return m_duration; }

    void setLoop(bool loop) { m_loop = loop; }
    bool getLoop() const { return m_loop; }

    void setPlayOnAwake(bool enabled) { m_playOnAwake = enabled; }
    bool getPlayOnAwake() const { return m_playOnAwake; }

    void setPlaybackSpeed(float speed) { m_playbackSpeed = std::max(speed, 0.0f); }
    float getPlaybackSpeed() const { return m_playbackSpeed; }

    void setEmitSignals(bool enabled) { m_emitSignals = enabled; }
    bool getEmitSignals() const { return m_emitSignals; }

    std::vector<Clip>& getClips() { return m_clips; }
    const std::vector<Clip>& getClips() const { return m_clips; }

    std::vector<Signal>& getSignals() { return m_signals; }
    const std::vector<Signal>& getSignals() const { return m_signals; }

private:

    static constexpr float kTimelineFps = 30.0f;

    struct ActiveClip
    {
        int clipIndex = -1;
        int animationIndex = -1;
        int track = 0;
        float localTime = 0.0f;
        float weight = 0.0f;
    };

    AnimationComponent* resolveAnimationComponent();
    std::vector<ActiveClip> collectActiveClips(float timeSeconds) const;
    void evaluateTimelinePose();
    void dispatchSignals(float previousTime, float currentTime, bool wrapped);
    void resetSignalState();
    void sortClipsByStartTime();

    static float evaluateCurve(BlendCurve curve, float t);
    static float computeClipEnvelopeWeight(const Clip& clip, float timelineTime);

    void drawTransportControls();
    void drawClipInspector();
    void drawSignalInspector();
    void drawSequencer();

    float frameToSeconds(int32_t frame) const;
    int32_t secondsToFrame(float seconds) const;

    AnimationComponent* m_animation = nullptr;

    bool m_playOnAwake = false;
    bool m_loop = true;
    bool m_emitSignals = true;
    float m_duration = 5.0f;
    float m_playbackSpeed = 1.0f;

    bool m_playing = false;
    bool m_paused = false;
    float m_currentTime = 0.0f;

    bool m_stateMachineOverridden = false;
    bool m_prevStateMachineEnabled = false;

    std::vector<Clip> m_clips;
    std::vector<Signal> m_signals;
    std::vector<bool> m_signalFired;

    int m_selectedClipIndex = -1;
    int m_selectedSignalIndex = -1;
    int32_t m_seqCurrentFrame = 0;
};
