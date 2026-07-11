#pragma once

#include "Component.h"
#include "Model/Model.h"

#include <unordered_map>

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

    enum class UpdateMethod : int32_t
    {
        GameTime = 0,
        UnscaledGameTime = 1,
        Manual = 2,
    };

    enum class WrapMode : int32_t
    {
        Hold = 0,
        Loop = 1,
        None = 2,
    };

    enum class BlendCurve : int32_t
    {
        Linear = 0,
        EaseIn = 1,
        EaseOut = 2,
        EaseInOut = 3,
        SmoothStep = 4,
    };

    struct Track
    {
        std::string name = "Track 0";
        std::string bindingObjectName;
        bool muted = false;
        bool solo = false;
        float weight = 1.0f;
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
    void evaluateAt(float seconds);

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

    void setInitialTime(float seconds) { m_initialTime = std::max(0.0f, seconds); }
    float getInitialTime() const { return m_initialTime; }

    void setUpdateMethod(UpdateMethod method) { m_updateMethod = method; }
    UpdateMethod getUpdateMethod() const { return m_updateMethod; }

    void setWrapMode(WrapMode mode) { m_wrapMode = mode; }
    WrapMode getWrapMode() const { return m_wrapMode; }

    void setPlaybackSpeed(float speed) { m_playbackSpeed = std::max(speed, 0.0f); }
    float getPlaybackSpeed() const { return m_playbackSpeed; }

    void setEmitSignals(bool enabled) { m_emitSignals = enabled; }
    bool getEmitSignals() const { return m_emitSignals; }

    const std::string& getAssetPath() const { return m_assetPath; }
    void setAssetPath(const std::string& path) { m_assetPath = path; }
    bool saveAsset() const;
    bool loadAsset(const std::string& path);
    bool reloadAsset();

    std::vector<Clip>& getClips() { return m_clips; }
    const std::vector<Clip>& getClips() const { return m_clips; }

    std::vector<Track>& getTracks() { return m_tracks; }
    const std::vector<Track>& getTracks() const { return m_tracks; }

    std::vector<Signal>& getSignals() { return m_signals; }
    const std::vector<Signal>& getSignals() const { return m_signals; }

private:

    static constexpr float kTimelineFps = 30.0f;

    struct ActiveClip
    {
        int clipIndex = -1;
        int track = 0;
        float localTime = 0.0f;
        float weight = 0.0f;
        AnimationComponent* animation = nullptr;
        uint64_t animationObjectId = 0;
        int animationIndex = -1;
    };

    AnimationComponent* resolveAnimationComponent();
    AnimationComponent* resolveTrackAnimationComponent(const Track& track) const;
    std::vector<ActiveClip> collectActiveClips(float timeSeconds) const;
    void evaluateTimelinePose();
    void dispatchSignals(float previousTime, float currentTime, bool wrapped);
    void resetSignalState();
    void sortClipsByStartTime();
    void ensureTrackExists(int trackIndex);
    void syncTracksFromClips();
    void cachePrePlayState();
    void restorePrePlayState();

    static float evaluateCurve(BlendCurve curve, float t);
    static float computeClipEnvelopeWeight(const Clip& clip, float timelineTime);

    void drawTransportControls();
    void drawTrackInspector();
    void drawClipInspector();
    void drawSignalInspector();
    void drawSequencer();

    float frameToSeconds(int32_t frame) const;
    int32_t secondsToFrame(float seconds) const;

    AnimationComponent* m_animation = nullptr;
    std::string m_assetPath;

    bool m_playOnAwake = false;
    bool m_loop = true;
    bool m_emitSignals = true;
    float m_duration = 5.0f;
    float m_initialTime = 0.0f;
    float m_playbackSpeed = 1.0f;
    UpdateMethod m_updateMethod = UpdateMethod::GameTime;
    WrapMode m_wrapMode = WrapMode::Hold;

    bool m_playing = false;
    bool m_paused = false;
    float m_currentTime = 0.0f;

    bool m_stateMachineOverridden = false;
    bool m_prevStateMachineEnabled = false;
    std::unordered_map<uint64_t, bool> m_prevStateMachineStates;

    std::vector<Clip> m_clips;
    std::vector<Track> m_tracks = { Track{} };
    std::vector<Signal> m_signals;
    std::vector<bool> m_signalFired;
    std::unordered_map<uint64_t, std::vector<Model::Bone>> m_prePlayPoses;

    int m_selectedClipIndex = -1;
    int m_selectedSignalIndex = -1;
    int32_t m_seqCurrentFrame = 0;
};
