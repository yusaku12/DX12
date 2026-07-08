#include "pch.h"
#include "TimelineComponent.h"

#include "AnimationComponent.h"
#include "GameObject/GameObject.h"
#include "System/EventBus.h"
#include "System/TimeManager.h"
#include "imgui_neo_sequencer.h"
#include <DirectXMath.h>
#include <unordered_map>

using namespace DirectX;

namespace
{
    constexpr float kTimelineFpsLocal = 30.0f;
    constexpr float kClipMinDuration = 1.0f / kTimelineFpsLocal;

    float positiveFmod(float value, float base)
    {
        if (base <= 0.0f)
        {
            return 0.0f;
        }

        const float mod = std::fmod(value, base);
        return (mod < 0.0f) ? (mod + base) : mod;
    }

    XMVECTOR normalizeSafeQuat(const Vector4& q)
    {
        XMVECTOR v = XMQuaternionNormalize(XMLoadFloat4(&q));
        if (XMVector4Equal(v, XMVectorZero()))
        {
            return XMQuaternionIdentity();
        }
        return v;
    }
}

void TimelineComponent::awake()
{
    resolveAnimationComponent();

    if (m_playOnAwake)
    {
        play();
    }
}

void TimelineComponent::update()
{
    resolveAnimationComponent();

    if (!m_playing || m_paused || m_duration <= 0.0f)
    {
        return;
    }

    const float deltaTime = std::max(0.0f, TimeManager::Instance().getDeltaTime() * m_playbackSpeed);
    if (deltaTime <= 0.0f)
    {
        return;
    }

    const float previousTime = m_currentTime;
    bool wrapped = false;

    m_currentTime += deltaTime;
    if (m_currentTime > m_duration)
    {
        if (m_loop)
        {
            m_currentTime = positiveFmod(m_currentTime, m_duration);
            wrapped = true;
            resetSignalState();
        }
        else
        {
            m_currentTime = m_duration;
            m_playing = false;
            m_paused = true;
        }
    }

    if (m_emitSignals)
    {
        dispatchSignals(previousTime, m_currentTime, wrapped);
    }

    evaluateTimelinePose();
}

void TimelineComponent::onDisable()
{
    stop(false);
}

void TimelineComponent::onDestroy()
{
    stop(false);
}

void TimelineComponent::play()
{
    resolveAnimationComponent();

    if (m_animation && !m_stateMachineOverridden)
    {
        m_prevStateMachineEnabled = m_animation->isStateMachineEnabled();
        m_animation->setStateMachineEnabled(false);
        m_stateMachineOverridden = true;
    }

    if (m_currentTime >= m_duration)
    {
        m_currentTime = 0.0f;
    }

    m_playing = true;
    m_paused = false;
    resetSignalState();
    evaluateTimelinePose();
}

void TimelineComponent::pause()
{
    if (!m_playing)
    {
        return;
    }

    m_paused = true;
}

void TimelineComponent::resume()
{
    if (!m_playing)
    {
        return;
    }

    m_paused = false;
}

void TimelineComponent::stop(bool resetTime)
{
    m_playing = false;
    m_paused = false;

    if (resetTime)
    {
        m_currentTime = 0.0f;
    }

    if (m_animation && m_stateMachineOverridden)
    {
        m_animation->setStateMachineEnabled(m_prevStateMachineEnabled);
    }

    m_stateMachineOverridden = false;
}

void TimelineComponent::setCurrentTime(float seconds)
{
    m_currentTime = std::clamp(seconds, 0.0f, m_duration);
    evaluateTimelinePose();
}

AnimationComponent* TimelineComponent::resolveAnimationComponent()
{
    if (!gameObject())
    {
        m_animation = nullptr;
        return nullptr;
    }

    m_animation = gameObject()->getComponent<AnimationComponent>();
    return m_animation;
}

std::vector<TimelineComponent::ActiveClip> TimelineComponent::collectActiveClips(float timeSeconds) const
{
    std::vector<ActiveClip> active;
    if (!m_animation)
    {
        return active;
    }

    for (int i = 0; i < static_cast<int>(m_clips.size()); ++i)
    {
        const Clip& clip = m_clips[i];
        if (!clip.enabled || clip.animationName.empty())
        {
            continue;
        }

        const float start = std::max(0.0f, clip.startTime);
        const float duration = std::max(kClipMinDuration, clip.duration);
        const float end = start + duration;
        if (timeSeconds < start || timeSeconds > end)
        {
            continue;
        }

        const int animationIndex = m_animation->findAnimationIndex(clip.animationName);
        if (animationIndex < 0)
        {
            continue;
        }

        const float animationLength = m_animation->getAnimationLength(animationIndex);
        if (animationLength <= 0.0f)
        {
            continue;
        }

        float localTime = (timeSeconds - start) * std::max(0.0f, clip.speed) + std::max(0.0f, clip.clipInTime);
        if (clip.loop)
        {
            localTime = positiveFmod(localTime, animationLength);
        }
        else
        {
            const float clipWindowEnd = std::max(0.0f, duration * std::max(0.0f, clip.speed));
            localTime = std::clamp(localTime, 0.0f, animationLength);
            localTime = std::min(localTime, std::max(0.0f, clip.clipInTime) + clipWindowEnd);
        }

        ActiveClip sample;
        sample.clipIndex = i;
        sample.animationIndex = animationIndex;
        sample.track = clip.track;
        sample.localTime = localTime;
        sample.weight = computeClipEnvelopeWeight(clip, timeSeconds);

        if (sample.weight > 0.00001f)
        {
            active.push_back(std::move(sample));
        }
    }

    return active;
}

void TimelineComponent::evaluateTimelinePose()
{
    if (!m_animation)
    {
        return;
    }

    std::vector<ActiveClip> active = collectActiveClips(m_currentTime);
    if (active.empty())
    {
        return;
    }

    struct PoseSample
    {
        int track = 0;
        float weight = 0.0f;
        std::vector<Model::Bone> pose;
    };

    std::vector<PoseSample> samples;
    samples.reserve(active.size());

    for (const ActiveClip& item : active)
    {
        PoseSample sample;
        sample.track = item.track;
        sample.weight = std::max(0.0f, item.weight);
        if (!m_animation->evaluateAnimationPose(item.animationIndex, item.localTime, sample.pose))
        {
            continue;
        }
        samples.push_back(std::move(sample));
    }

    if (samples.empty())
    {
        return;
    }

    float totalWeight = 0.0f;
    for (const auto& s : samples)
    {
        totalWeight += s.weight;
    }

    if (totalWeight <= 0.00001f)
    {
        return;
    }

    std::vector<Model::Bone> blended = samples.front().pose;
    const size_t boneCount = blended.size();

    for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
    {
        XMVECTOR sumScale = XMVectorZero();
        XMVECTOR sumTranslate = XMVectorZero();
        XMVECTOR sumQuat = XMVectorZero();

        XMVECTOR refQuat = normalizeSafeQuat(samples.front().pose[boneIndex].rotate);

        for (const auto& sample : samples)
        {
            if (boneIndex >= sample.pose.size())
            {
                continue;
            }

            const float normalizedWeight = sample.weight / totalWeight;
            if (normalizedWeight <= 0.0f)
            {
                continue;
            }

            const XMVECTOR scale = XMLoadFloat3(&sample.pose[boneIndex].scale);
            const XMVECTOR translate = XMLoadFloat3(&sample.pose[boneIndex].translate);
            XMVECTOR quat = normalizeSafeQuat(sample.pose[boneIndex].rotate);

            if (XMVectorGetX(XMVector4Dot(refQuat, quat)) < 0.0f)
            {
                quat = XMVectorNegate(quat);
            }

            const XMVECTOR weight = XMVectorReplicate(normalizedWeight);
            sumScale = XMVectorAdd(sumScale, XMVectorMultiply(scale, weight));
            sumTranslate = XMVectorAdd(sumTranslate, XMVectorMultiply(translate, weight));
            sumQuat = XMVectorAdd(sumQuat, XMVectorMultiply(quat, weight));
        }

        XMStoreFloat3(&blended[boneIndex].scale, sumScale);
        XMStoreFloat3(&blended[boneIndex].translate, sumTranslate);

        XMVECTOR normalizedQuat = XMQuaternionNormalize(sumQuat);
        if (XMVector4Equal(normalizedQuat, XMVectorZero()))
        {
            normalizedQuat = refQuat;
        }
        XMStoreFloat4(&blended[boneIndex].rotate, normalizedQuat);
    }

    m_animation->applyPose(blended, true);
}

void TimelineComponent::dispatchSignals(float previousTime, float currentTime, bool wrapped)
{
    if (m_signals.empty())
    {
        return;
    }

    if (m_signalFired.size() != m_signals.size())
    {
        m_signalFired.assign(m_signals.size(), false);
    }

    const uint64_t senderId = gameObject() ? gameObject()->getInstanceId() : getInstanceId();

    for (size_t i = 0; i < m_signals.size(); ++i)
    {
        const Signal& signal = m_signals[i];
        if (!signal.enabled || signal.eventName.empty() || m_signalFired[i])
        {
            continue;
        }

        const float signalTime = std::clamp(signal.time, 0.0f, m_duration);

        bool fire = false;
        if (!wrapped)
        {
            fire = (signalTime > previousTime) && (signalTime <= currentTime);
        }
        else
        {
            fire = (signalTime > previousTime) || (signalTime <= currentTime);
        }

        if (fire)
        {
            EventBus::Instance().publish(signal.eventName, senderId);
            m_signalFired[i] = true;
        }
    }
}

void TimelineComponent::resetSignalState()
{
    m_signalFired.assign(m_signals.size(), false);
}

void TimelineComponent::sortClipsByStartTime()
{
    std::sort(
        m_clips.begin(),
        m_clips.end(),
        [](const Clip& a, const Clip& b)
        {
            if (a.track != b.track)
            {
                return a.track < b.track;
            }
            if (a.startTime == b.startTime)
            {
                return a.animationName < b.animationName;
            }
            return a.startTime < b.startTime;
        });
}

float TimelineComponent::evaluateCurve(BlendCurve curve, float t)
{
    const float x = std::clamp(t, 0.0f, 1.0f);

    switch (curve)
    {
    case BlendCurve::Linear:
        return x;
    case BlendCurve::EaseIn:
        return x * x;
    case BlendCurve::EaseOut:
        return 1.0f - (1.0f - x) * (1.0f - x);
    case BlendCurve::EaseInOut:
        return (x < 0.5f) ? (2.0f * x * x) : (1.0f - std::pow(-2.0f * x + 2.0f, 2.0f) * 0.5f);
    case BlendCurve::SmoothStep:
        return x * x * (3.0f - 2.0f * x);
    default:
        return x;
    }
}

float TimelineComponent::computeClipEnvelopeWeight(const Clip& clip, float timelineTime)
{
    const float start = std::max(0.0f, clip.startTime);
    const float duration = std::max(kClipMinDuration, clip.duration);
    const float end = start + duration;

    if (timelineTime < start || timelineTime > end)
    {
        return 0.0f;
    }

    float weight = std::max(0.0f, clip.weight);
    if (weight <= 0.0f)
    {
        return 0.0f;
    }

    const float fromStart = timelineTime - start;
    const float toEnd = end - timelineTime;

    if (clip.blendIn > 0.0f)
    {
        const float inT = std::clamp(fromStart / std::max(clip.blendIn, 0.00001f), 0.0f, 1.0f);
        weight *= evaluateCurve(clip.blendInCurve, inT);
    }

    if (clip.blendOut > 0.0f)
    {
        const float outT = std::clamp(toEnd / std::max(clip.blendOut, 0.00001f), 0.0f, 1.0f);
        weight *= evaluateCurve(clip.blendOutCurve, outT);
    }

    return std::max(0.0f, weight);
}

void TimelineComponent::drawTransportControls()
{
    ImGui::Checkbox("Play On Awake", &m_playOnAwake);
    ImGui::SameLine();
    ImGui::Checkbox("Loop Timeline", &m_loop);
    ImGui::SameLine();
    ImGui::Checkbox("Emit Signals", &m_emitSignals);

    ImGui::DragFloat("Duration", &m_duration, 0.05f, 0.05f, 600.0f, "%.2f s");
    ImGui::DragFloat("Playback Speed", &m_playbackSpeed, 0.01f, 0.0f, 8.0f, "%.2f");

    if (!m_playing)
    {
        if (ImGui::Button("Play"))
        {
            play();
        }
    }
    else if (m_paused)
    {
        if (ImGui::Button("Resume"))
        {
            resume();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            stop(true);
            evaluateTimelinePose();
        }
    }
    else
    {
        if (ImGui::Button("Pause"))
        {
            pause();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            stop(true);
            evaluateTimelinePose();
        }
    }

    float timelineTime = m_currentTime;
    if (ImGui::SliderFloat("Time", &timelineTime, 0.0f, std::max(0.05f, m_duration), "%.2f s"))
    {
        m_currentTime = timelineTime;
        evaluateTimelinePose();
    }
}

void TimelineComponent::drawClipInspector()
{
    ImGui::SeparatorText("Clips");

    if (ImGui::Button("Add Clip"))
    {
        Clip clip;
        if (m_animation && m_animation->getAnimationCount() > 0)
        {
            clip.animationName = m_animation->getAnimationName(0);
        }
        clip.startTime = std::clamp(m_currentTime, 0.0f, m_duration);
        clip.duration = 1.0f;
        m_clips.push_back(std::move(clip));
        sortClipsByStartTime();
        m_selectedClipIndex = static_cast<int>(m_clips.size()) - 1;
    }

    const char* curveItems[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "SmoothStep" };

    for (int i = 0; i < static_cast<int>(m_clips.size()); ++i)
    {
        Clip& clip = m_clips[i];

        ImGui::PushID(i);
        const std::string label = std::format("Track {} Clip {}: {}", clip.track, i, clip.animationName.empty() ? "<None>" : clip.animationName);
        if (ImGui::TreeNode(label.c_str()))
        {
            m_selectedClipIndex = i;

            ImGui::Checkbox("Enabled", &clip.enabled);
            ImGui::DragInt("Track", &clip.track, 1.0f, 0, 32);
            ImGui::DragFloat("Weight", &clip.weight, 0.01f, 0.0f, 8.0f, "%.2f");
            ImGui::DragFloat("Start", &clip.startTime, 0.01f, 0.0f, m_duration, "%.2f s");
            ImGui::DragFloat("Duration", &clip.duration, 0.01f, kClipMinDuration, m_duration, "%.2f s");
            ImGui::DragFloat("Clip In", &clip.clipInTime, 0.01f, 0.0f, 3600.0f, "%.2f s");
            ImGui::DragFloat("Speed", &clip.speed, 0.01f, 0.0f, 8.0f, "%.2f");
            ImGui::Checkbox("Loop", &clip.loop);

            ImGui::SeparatorText("Blend");
            ImGui::DragFloat("Blend In", &clip.blendIn, 0.01f, 0.0f, 10.0f, "%.2f s");
            ImGui::DragFloat("Blend Out", &clip.blendOut, 0.01f, 0.0f, 10.0f, "%.2f s");
            int blendInCurve = static_cast<int>(clip.blendInCurve);
            if (ImGui::Combo("Blend In Curve", &blendInCurve, curveItems, IM_ARRAYSIZE(curveItems)))
            {
                clip.blendInCurve = static_cast<BlendCurve>(std::clamp(blendInCurve, 0, 4));
            }
            int blendOutCurve = static_cast<int>(clip.blendOutCurve);
            if (ImGui::Combo("Blend Out Curve", &blendOutCurve, curveItems, IM_ARRAYSIZE(curveItems)))
            {
                clip.blendOutCurve = static_cast<BlendCurve>(std::clamp(blendOutCurve, 0, 4));
            }

            std::string preview = clip.animationName.empty() ? "<None>" : clip.animationName;
            if (ImGui::BeginCombo("Animation", preview.c_str()))
            {
                if (m_animation)
                {
                    const int count = m_animation->getAnimationCount();
                    for (int anim = 0; anim < count; ++anim)
                    {
                        const std::string name = m_animation->getAnimationName(anim);
                        const bool selected = (name == clip.animationName);
                        if (ImGui::Selectable(name.c_str(), selected))
                        {
                            clip.animationName = name;
                        }
                    }
                }
                ImGui::EndCombo();
            }

            clip.startTime = std::clamp(clip.startTime, 0.0f, m_duration);
            clip.duration = std::max(kClipMinDuration, clip.duration);
            clip.clipInTime = std::max(0.0f, clip.clipInTime);
            clip.speed = std::max(0.0f, clip.speed);
            clip.weight = std::max(0.0f, clip.weight);
            clip.blendIn = std::max(0.0f, clip.blendIn);
            clip.blendOut = std::max(0.0f, clip.blendOut);

            if (ImGui::Button("Remove Clip"))
            {
                m_clips.erase(m_clips.begin() + i);
                m_selectedClipIndex = -1;
                ImGui::TreePop();
                ImGui::PopID();
                sortClipsByStartTime();
                break;
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void TimelineComponent::drawSignalInspector()
{
    ImGui::SeparatorText("Signals");

    if (ImGui::Button("Add Signal"))
    {
        Signal signal;
        signal.time = std::clamp(m_currentTime, 0.0f, m_duration);
        signal.eventName = "Timeline.Signal";
        m_signals.push_back(std::move(signal));
        resetSignalState();
        m_selectedSignalIndex = static_cast<int>(m_signals.size()) - 1;
    }

    for (int i = 0; i < static_cast<int>(m_signals.size()); ++i)
    {
        Signal& signal = m_signals[i];

        ImGui::PushID(10000 + i);
        const std::string label = std::format("Signal {}: {}", i, signal.eventName.empty() ? "<Unnamed>" : signal.eventName);
        if (ImGui::TreeNode(label.c_str()))
        {
            m_selectedSignalIndex = i;

            ImGui::Checkbox("Enabled", &signal.enabled);
            ImGui::DragFloat("Time", &signal.time, 0.01f, 0.0f, m_duration, "%.2f s");

            char eventBuffer[128]{};
            std::snprintf(eventBuffer, sizeof(eventBuffer), "%s", signal.eventName.c_str());
            if (ImGui::InputText("Event Name", eventBuffer, IM_ARRAYSIZE(eventBuffer)))
            {
                signal.eventName = eventBuffer;
            }

            signal.time = std::clamp(signal.time, 0.0f, m_duration);

            if (ImGui::Button("Test Fire"))
            {
                const uint64_t senderId = gameObject() ? gameObject()->getInstanceId() : getInstanceId();
                if (!signal.eventName.empty())
                {
                    EventBus::Instance().publish(signal.eventName, senderId);
                }
            }

            if (ImGui::Button("Remove Signal"))
            {
                m_signals.erase(m_signals.begin() + i);
                m_selectedSignalIndex = -1;
                resetSignalState();
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void TimelineComponent::drawSequencer()
{
    int32_t startFrame = 0;
    int32_t endFrame = std::max(1, secondsToFrame(std::max(m_duration, 0.05f)));
    const int32_t previousFrame = secondsToFrame(m_currentTime);

    m_seqCurrentFrame = previousFrame;

    if (ImGui::BeginNeoSequencer(
        "##TimelineSequencer",
        &m_seqCurrentFrame,
        &startFrame,
        &endFrame,
        ImVec2(0, 260),
        ImGuiNeoSequencerFlags_AlwaysShowHeader))
    {
        std::unordered_map<int, std::vector<int>> clipsByTrack;
        for (int i = 0; i < static_cast<int>(m_clips.size()); ++i)
        {
            clipsByTrack[m_clips[i].track].push_back(i);
        }

        for (auto& [track, clipIndices] : clipsByTrack)
        {
            std::string trackLabel = std::format("Track {}##track{}", track, track);
            if (ImGui::BeginNeoTimelineEx(trackLabel.c_str()))
            {
                for (int clipIndex : clipIndices)
                {
                    Clip& clip = m_clips[clipIndex];
                    int32_t clipStartFrame = secondsToFrame(clip.startTime);
                    int32_t clipEndFrame = secondsToFrame(clip.startTime + std::max(kClipMinDuration, clip.duration));

                    ImGui::NeoKeyframe(&clipStartFrame);
                    ImGui::NeoKeyframe(&clipEndFrame);

                    clip.startTime = std::clamp(frameToSeconds(clipStartFrame), 0.0f, m_duration);
                    clip.duration = std::max(kClipMinDuration, frameToSeconds(std::max(clipEndFrame - clipStartFrame, 1)));
                }
                ImGui::EndNeoTimeLine();
            }
        }

        if (ImGui::BeginNeoTimelineEx("Signals##signals"))
        {
            for (int i = 0; i < static_cast<int>(m_signals.size()); ++i)
            {
                Signal& signal = m_signals[i];
                int32_t signalFrame = secondsToFrame(signal.time);
                ImGui::NeoKeyframe(&signalFrame);
                signal.time = std::clamp(frameToSeconds(signalFrame), 0.0f, m_duration);
            }
            ImGui::EndNeoTimeLine();
        }

        ImGui::EndNeoSequencer();
    }

    if (m_seqCurrentFrame != previousFrame)
    {
        m_currentTime = std::clamp(frameToSeconds(m_seqCurrentFrame), 0.0f, m_duration);
        evaluateTimelinePose();
    }
}

void TimelineComponent::inspectGUI()
{
    if (!m_animation)
    {
        ImGui::TextDisabled("AnimationComponent が見つかりません。Timeline は同一 GameObject 上の AnimationComponent を制御します。");
    }

    drawTransportControls();

    ImGui::SeparatorText("Sequencer");
    drawSequencer();

    drawClipInspector();
    drawSignalInspector();

    if (ImGui::Button("Sort Clips By Track/Start"))
    {
        sortClipsByStartTime();
    }
}

float TimelineComponent::frameToSeconds(int32_t frame) const
{
    return static_cast<float>(frame) / kTimelineFps;
}

int32_t TimelineComponent::secondsToFrame(float seconds) const
{
    return static_cast<int32_t>(std::round(std::max(0.0f, seconds) * kTimelineFps));
}
