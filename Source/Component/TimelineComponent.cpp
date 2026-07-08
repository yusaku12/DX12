#include "pch.h"
#include "TimelineComponent.h"

#include "AnimationComponent.h"
#include "GameObject/GameObject.h"
#include "GameObject/GameObjectRegistry.h"
#include "System/EventBus.h"
#include "System/TimeManager.h"
#include "TimelineAsset.h"
#include "imgui_neo_sequencer.h"
#include <DirectXMath.h>
#include <unordered_map>

using namespace DirectX;

namespace
{
    constexpr float kClipMinDuration = 1.0f / 30.0f;

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
    syncTracksFromClips();

    if (!m_assetPath.empty())
    {
        reloadAsset();
    }

    if (m_playOnAwake)
    {
        play();
    }
}

void TimelineComponent::update()
{
    resolveAnimationComponent();
    syncTracksFromClips();

    if (!m_playing || m_paused || m_duration <= 0.0f)
    {
        return;
    }

    float deltaTime = 0.0f;
    switch (m_updateMethod)
    {
    case UpdateMethod::GameTime:
        deltaTime = TimeManager::Instance().getDeltaTime();
        break;
    case UpdateMethod::UnscaledGameTime:
        deltaTime = TimeManager::Instance().getUnscaledDeltaTime();
        break;
    case UpdateMethod::Manual:
        deltaTime = 0.0f;
        break;
    }

    deltaTime = std::max(0.0f, deltaTime * m_playbackSpeed);
    if (deltaTime <= 0.0f)
    {
        return;
    }

    const float previousTime = m_currentTime;
    bool wrapped = false;

    m_currentTime += deltaTime;
    if (m_currentTime > m_duration)
    {
        switch (m_wrapMode)
        {
        case WrapMode::Loop:
            m_currentTime = positiveFmod(m_currentTime, m_duration);
            wrapped = true;
            resetSignalState();
            break;
        case WrapMode::Hold:
            m_currentTime = m_duration;
            m_playing = false;
            m_paused = true;
            break;
        case WrapMode::None:
            m_currentTime = m_duration;
            evaluateTimelinePose();
            restorePrePlayState();
            m_playing = false;
            m_paused = false;
            return;
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
    syncTracksFromClips();
    cachePrePlayState();

    if (m_currentTime <= 0.0f)
    {
        m_currentTime = std::clamp(m_initialTime, 0.0f, m_duration);
    }
    else if (m_currentTime >= m_duration)
    {
        m_currentTime = std::clamp(m_initialTime, 0.0f, m_duration);
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

    if (m_wrapMode == WrapMode::None)
    {
        restorePrePlayState();
    }
    else
    {
        m_prevStateMachineStates.clear();
        m_prePlayPoses.clear();
        m_stateMachineOverridden = false;
    }

    if (resetTime)
    {
        m_currentTime = std::clamp(m_initialTime, 0.0f, m_duration);
    }
}

void TimelineComponent::evaluateAt(float seconds)
{
    m_currentTime = std::clamp(seconds, 0.0f, m_duration);
    evaluateTimelinePose();
}

void TimelineComponent::setCurrentTime(float seconds)
{
    evaluateAt(seconds);
}

bool TimelineComponent::saveAsset() const
{
    if (m_assetPath.empty())
    {
        LOG_WARN("[TimelineComponent] Save asset skipped. Asset path is empty.");
        return false;
    }

    std::filesystem::path path = m_assetPath;
    std::error_code ec;
    std::filesystem::path parent = path.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, ec);
    }

    return TimelineAsset::save(path, m_duration, m_tracks, m_clips, m_signals);
}

bool TimelineComponent::loadAsset(const std::string& path)
{
    float duration = 0.0f;
    std::vector<Track> tracks;
    std::vector<Clip> clips;
    std::vector<Signal> signals;
    if (!TimelineAsset::load(path, duration, tracks, clips, signals))
    {
        return false;
    }

    m_assetPath = path;
    m_duration = std::max(duration, 0.05f);
    m_tracks = std::move(tracks);
    m_clips = std::move(clips);
    m_signals = std::move(signals);
    syncTracksFromClips();
    resetSignalState();
    return true;
}

bool TimelineComponent::reloadAsset()
{
    if (m_assetPath.empty())
    {
        return false;
    }
    return loadAsset(m_assetPath);
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

AnimationComponent* TimelineComponent::resolveTrackAnimationComponent(const Track& track) const
{
    if (track.bindingObjectName.empty())
    {
        return const_cast<TimelineComponent*>(this)->m_animation;
    }

    for (GameObject* obj : GameObjectRegistry::Instance().getAll())
    {
        if (!obj || obj->isDestroyed())
        {
            continue;
        }

        if (obj->getName() != track.bindingObjectName)
        {
            continue;
        }

        return obj->getComponent<AnimationComponent>();
    }

    return nullptr;
}

std::vector<TimelineComponent::ActiveClip> TimelineComponent::collectActiveClips(float timeSeconds) const
{
    std::vector<ActiveClip> active;

    bool anySolo = false;
    for (const Track& track : m_tracks)
    {
        if (!track.muted && track.solo)
        {
            anySolo = true;
            break;
        }
    }

    for (int i = 0; i < static_cast<int>(m_clips.size()); ++i)
    {
        const Clip& clip = m_clips[i];
        if (!clip.enabled || clip.animationName.empty())
        {
            continue;
        }

        if (clip.track < 0 || clip.track >= static_cast<int>(m_tracks.size()))
        {
            continue;
        }

        const Track& track = m_tracks[clip.track];
        if (track.muted)
        {
            continue;
        }
        if (anySolo && !track.solo)
        {
            continue;
        }

        AnimationComponent* targetAnimation = resolveTrackAnimationComponent(track);
        if (!targetAnimation)
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

        const int animationIndex = targetAnimation->findAnimationIndex(clip.animationName);
        if (animationIndex < 0)
        {
            continue;
        }

        const float animationLength = targetAnimation->getAnimationLength(animationIndex);
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
        sample.track = clip.track;
        sample.localTime = localTime;
        sample.weight = computeClipEnvelopeWeight(clip, timeSeconds);
        sample.animation = targetAnimation;
        sample.animationObjectId = targetAnimation->gameObject() ? targetAnimation->gameObject()->getInstanceId() : targetAnimation->getInstanceId();
        sample.animationIndex = animationIndex;

        if (sample.weight > 0.00001f)
        {
            active.push_back(std::move(sample));
        }
    }

    return active;
}

void TimelineComponent::evaluateTimelinePose()
{
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

    std::unordered_map<uint64_t, std::vector<ActiveClip>> clipsByTarget;
    for (const auto& clip : active)
    {
        clipsByTarget[clip.animationObjectId].push_back(clip);
    }

    for (auto& [targetId, targetClips] : clipsByTarget)
    {
        if (targetClips.empty() || !targetClips.front().animation)
        {
            continue;
        }

        AnimationComponent* targetAnimation = targetClips.front().animation;
        std::vector<PoseSample> samples;
        samples.reserve(targetClips.size());

        for (const ActiveClip& item : targetClips)
        {
            PoseSample sample;
            sample.track = item.track;
            sample.weight = std::max(0.0f, item.weight);
            if (!targetAnimation->evaluateAnimationPose(item.animationIndex, item.localTime, sample.pose))
            {
                continue;
            }
            samples.push_back(std::move(sample));
        }

        if (samples.empty())
        {
            continue;
        }

        std::unordered_map<int, std::vector<PoseSample>> samplesByTrack;
        for (auto& sample : samples)
        {
            samplesByTrack[sample.track].push_back(std::move(sample));
        }

        std::vector<PoseSample> blendedTracks;
        blendedTracks.reserve(samplesByTrack.size());

        for (auto& [trackIndex, trackSamples] : samplesByTrack)
        {
            float trackTotalWeight = 0.0f;
            for (const auto& sample : trackSamples)
            {
                trackTotalWeight += sample.weight;
            }

            if (trackTotalWeight <= 0.00001f)
            {
                continue;
            }

            PoseSample trackPose;
            trackPose.track = trackIndex;
            trackPose.weight = (trackIndex >= 0 && trackIndex < static_cast<int>(m_tracks.size()))
                ? std::max(0.0f, m_tracks[trackIndex].weight)
                : 1.0f;
            trackPose.pose = trackSamples.front().pose;

            const size_t boneCount = trackPose.pose.size();
            for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
            {
                XMVECTOR sumScale = XMVectorZero();
                XMVECTOR sumTranslate = XMVectorZero();
                XMVECTOR sumQuat = XMVectorZero();
                XMVECTOR refQuat = normalizeSafeQuat(trackSamples.front().pose[boneIndex].rotate);

                for (const auto& sample : trackSamples)
                {
                    if (boneIndex >= sample.pose.size())
                    {
                        continue;
                    }

                    const float normalizedWeight = sample.weight / trackTotalWeight;
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

                XMStoreFloat3(&trackPose.pose[boneIndex].scale, sumScale);
                XMStoreFloat3(&trackPose.pose[boneIndex].translate, sumTranslate);

                XMVECTOR normalizedQuat = XMQuaternionNormalize(sumQuat);
                if (XMVector4Equal(normalizedQuat, XMVectorZero()))
                {
                    normalizedQuat = refQuat;
                }
                XMStoreFloat4(&trackPose.pose[boneIndex].rotate, normalizedQuat);
            }

            blendedTracks.push_back(std::move(trackPose));
        }

        if (blendedTracks.empty())
        {
            continue;
        }

        std::sort(
            blendedTracks.begin(),
            blendedTracks.end(),
            [](const PoseSample& a, const PoseSample& b)
            {
                return a.track < b.track;
            });

        float totalTrackWeight = 0.0f;
        for (const auto& track : blendedTracks)
        {
            totalTrackWeight += track.weight;
        }

        if (totalTrackWeight <= 0.00001f)
        {
            continue;
        }

        std::vector<Model::Bone> blended = blendedTracks.front().pose;
        const size_t boneCount = blended.size();

        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            XMVECTOR sumScale = XMVectorZero();
            XMVECTOR sumTranslate = XMVectorZero();
            XMVECTOR sumQuat = XMVectorZero();
            XMVECTOR refQuat = normalizeSafeQuat(blendedTracks.front().pose[boneIndex].rotate);

            for (const auto& sample : blendedTracks)
            {
                if (boneIndex >= sample.pose.size())
                {
                    continue;
                }

                const float normalizedWeight = sample.weight / totalTrackWeight;
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

        targetAnimation->applyPose(blended, true);
    }
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

void TimelineComponent::ensureTrackExists(int trackIndex)
{
    if (trackIndex < 0)
    {
        return;
    }

    while (trackIndex >= static_cast<int>(m_tracks.size()))
    {
        Track track;
        track.name = std::format("Track {}", m_tracks.size());
        m_tracks.push_back(std::move(track));
    }
}

void TimelineComponent::syncTracksFromClips()
{
    if (m_tracks.empty())
    {
        m_tracks.push_back(Track{});
    }

    for (Clip& clip : m_clips)
    {
        clip.track = std::max(0, clip.track);
        ensureTrackExists(clip.track);
    }

    for (size_t i = 0; i < m_tracks.size(); ++i)
    {
        if (m_tracks[i].name.empty())
        {
            m_tracks[i].name = std::format("Track {}", i);
        }
        m_tracks[i].weight = std::max(0.0f, m_tracks[i].weight);
    }
}

void TimelineComponent::cachePrePlayState()
{
    if (m_stateMachineOverridden)
    {
        return;
    }

    m_prevStateMachineStates.clear();
    m_prePlayPoses.clear();

    auto snapshotAnimation = [this](AnimationComponent* anim)
    {
        if (!anim || !anim->gameObject())
        {
            return;
        }

        const uint64_t objectId = anim->gameObject()->getInstanceId();
        if (m_prevStateMachineStates.contains(objectId))
        {
            return;
        }

        m_prevStateMachineStates[objectId] = anim->isStateMachineEnabled();
        if (Model* model = anim->getModel())
        {
            m_prePlayPoses[objectId] = model->getBone();
        }
        anim->setStateMachineEnabled(false);
    };

    if (m_animation)
    {
        snapshotAnimation(m_animation);
    }

    for (const Track& track : m_tracks)
    {
        snapshotAnimation(resolveTrackAnimationComponent(track));
    }

    m_stateMachineOverridden = true;
}

void TimelineComponent::restorePrePlayState()
{
    for (GameObject* obj : GameObjectRegistry::Instance().getAll())
    {
        if (!obj || obj->isDestroyed())
        {
            continue;
        }

        auto* anim = obj->getComponent<AnimationComponent>();
        if (!anim)
        {
            continue;
        }

        const uint64_t objectId = obj->getInstanceId();
        auto stateIt = m_prevStateMachineStates.find(objectId);
        if (stateIt != m_prevStateMachineStates.end())
        {
            anim->setStateMachineEnabled(stateIt->second);
        }

        auto poseIt = m_prePlayPoses.find(objectId);
        if (poseIt != m_prePlayPoses.end() && !poseIt->second.empty())
        {
            anim->applyPose(poseIt->second, true);
        }
    }

    m_prevStateMachineStates.clear();
    m_prePlayPoses.clear();
    m_stateMachineOverridden = false;
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
    syncTracksFromClips();

    ImGui::Checkbox("Play On Awake", &m_playOnAwake);
    ImGui::SameLine();
    ImGui::Checkbox("Emit Signals", &m_emitSignals);

    ImGui::DragFloat("Duration", &m_duration, 0.05f, 0.05f, 600.0f, "%.2f s");
    ImGui::DragFloat("Initial Time", &m_initialTime, 0.05f, 0.0f, std::max(0.05f, m_duration), "%.2f s");
    ImGui::DragFloat("Playback Speed", &m_playbackSpeed, 0.01f, 0.0f, 8.0f, "%.2f");

    int updateMethod = static_cast<int>(m_updateMethod);
    const char* updateMethods[] = { "Game Time", "Unscaled Game Time", "Manual" };
    if (ImGui::Combo("Update Method", &updateMethod, updateMethods, IM_ARRAYSIZE(updateMethods)))
    {
        m_updateMethod = static_cast<UpdateMethod>(std::clamp(updateMethod, 0, 2));
    }

    int wrapMode = static_cast<int>(m_wrapMode);
    const char* wrapModes[] = { "Hold", "Loop", "None" };
    if (ImGui::Combo("Wrap Mode", &wrapMode, wrapModes, IM_ARRAYSIZE(wrapModes)))
    {
        m_wrapMode = static_cast<WrapMode>(std::clamp(wrapMode, 0, 2));
        m_loop = (m_wrapMode == WrapMode::Loop);
    }

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
    if (ImGui::SliderFloat("Current Time", &timelineTime, 0.0f, std::max(0.05f, m_duration), "%.2f s"))
    {
        m_currentTime = timelineTime;
        evaluateTimelinePose();
    }

    char assetPathBuf[512] = {};
    std::snprintf(assetPathBuf, sizeof(assetPathBuf), "%s", m_assetPath.c_str());
    if (ImGui::InputText("Timeline Asset", assetPathBuf, IM_ARRAYSIZE(assetPathBuf)))
    {
        m_assetPath = assetPathBuf;
    }
    if (ImGui::Button("Load Asset"))
    {
        if (!m_assetPath.empty())
        {
            loadAsset(m_assetPath);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Asset"))
    {
        saveAsset();
    }
}

void TimelineComponent::drawTrackInspector()
{
    ImGui::SeparatorText("Tracks");

    if (ImGui::Button("Add Track"))
    {
        Track track;
        track.name = std::format("Track {}", m_tracks.size());
        m_tracks.push_back(std::move(track));
    }

    const auto& objects = GameObjectRegistry::Instance().getAll();

    for (int i = 0; i < static_cast<int>(m_tracks.size()); ++i)
    {
        Track& track = m_tracks[i];
        ImGui::PushID(5000 + i);

        const std::string label = std::format("Track {}: {}", i, track.name);
        if (ImGui::TreeNode(label.c_str()))
        {
            char nameBuffer[128] = {};
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", track.name.c_str());
            if (ImGui::InputText("Name", nameBuffer, IM_ARRAYSIZE(nameBuffer)))
            {
                track.name = nameBuffer;
            }

            ImGui::Checkbox("Muted", &track.muted);
            ImGui::SameLine();
            ImGui::Checkbox("Solo", &track.solo);
            ImGui::DragFloat("Track Weight", &track.weight, 0.01f, 0.0f, 8.0f, "%.2f");

            const char* bindingLabel = track.bindingObjectName.empty() ? "<Self>" : track.bindingObjectName.c_str();
            if (ImGui::BeginCombo("Binding", bindingLabel))
            {
                const bool selfSelected = track.bindingObjectName.empty();
                if (ImGui::Selectable("<Self>", selfSelected))
                {
                    track.bindingObjectName.clear();
                }

                for (GameObject* obj : objects)
                {
                    if (!obj || obj->isDestroyed())
                    {
                        continue;
                    }

                    if (!obj->getComponent<AnimationComponent>())
                    {
                        continue;
                    }

                    const bool selected = (obj->getName() == track.bindingObjectName);
                    if (ImGui::Selectable(obj->getName().c_str(), selected))
                    {
                        track.bindingObjectName = obj->getName();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

void TimelineComponent::drawClipInspector()
{
    ImGui::SeparatorText("Clips");

    if (ImGui::Button("Add Clip"))
    {
        Clip clip;
        clip.track = 0;
        if (AnimationComponent* targetAnim = resolveTrackAnimationComponent(m_tracks[0]))
        {
            if (targetAnim->getAnimationCount() > 0)
            {
                clip.animationName = targetAnim->getAnimationName(0);
            }
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
            ensureTrackExists(clip.track);
            if (ImGui::BeginCombo("Track", m_tracks[clip.track].name.c_str()))
            {
                for (int trackIndex = 0; trackIndex < static_cast<int>(m_tracks.size()); ++trackIndex)
                {
                    const bool selected = (trackIndex == clip.track);
                    if (ImGui::Selectable(m_tracks[trackIndex].name.c_str(), selected))
                    {
                        clip.track = trackIndex;
                    }
                }
                ImGui::EndCombo();
            }

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

            AnimationComponent* targetAnim = resolveTrackAnimationComponent(m_tracks[clip.track]);
            std::string preview = clip.animationName.empty() ? "<None>" : clip.animationName;
            if (ImGui::BeginCombo("Animation", preview.c_str()))
            {
                if (targetAnim)
                {
                    const int count = targetAnim->getAnimationCount();
                    for (int anim = 0; anim < count; ++anim)
                    {
                        const std::string name = targetAnim->getAnimationName(anim);
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

            char eventBuffer[128] = {};
            std::snprintf(eventBuffer, sizeof(eventBuffer), "%s", signal.eventName.c_str());
            if (ImGui::InputText("Event Name", eventBuffer, IM_ARRAYSIZE(eventBuffer)))
            {
                signal.eventName = eventBuffer;
            }

            if (ImGui::Button("Test Fire") && !signal.eventName.empty())
            {
                const uint64_t senderId = gameObject() ? gameObject()->getInstanceId() : getInstanceId();
                EventBus::Instance().publish(signal.eventName, senderId);
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
    syncTracksFromClips();

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
            ensureTrackExists(track);
            const Track& trackData = m_tracks[track];
            std::string trackLabel = std::format("{}{}{}##track{}",
                trackData.muted ? "[M] " : "",
                trackData.solo ? "[S] " : "",
                trackData.name,
                track);
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
    drawTransportControls();
    ImGui::SeparatorText("Sequencer");
    drawSequencer();
    drawTrackInspector();
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
