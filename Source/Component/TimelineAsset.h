#pragma once

#include "TimelineComponent.h"

namespace TimelineAsset
{
    bool save(
        const std::filesystem::path& filePath,
        float duration,
        const std::vector<TimelineComponent::Track>& tracks,
        const std::vector<TimelineComponent::Clip>& clips,
        const std::vector<TimelineComponent::Signal>& signals);

    bool load(
        const std::filesystem::path& filePath,
        float& outDuration,
        std::vector<TimelineComponent::Track>& outTracks,
        std::vector<TimelineComponent::Clip>& outClips,
        std::vector<TimelineComponent::Signal>& outSignals);
}
