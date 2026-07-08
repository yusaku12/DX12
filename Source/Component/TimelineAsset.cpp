#include "pch.h"
#include "TimelineAsset.h"

namespace
{
    constexpr int kTimelineAssetVersion = 1;

    struct TokenReader
    {
        explicit TokenReader(std::istream& input)
            : in(input)
        {
        }

        bool expect(const char* token)
        {
            std::string value;
            if (!(in >> value)) return false;
            return value == token;
        }

        bool readString(std::string& out)
        {
            return static_cast<bool>(in >> std::quoted(out));
        }

        bool readInt(int& out)
        {
            return static_cast<bool>(in >> out);
        }

        bool readFloat(float& out)
        {
            return static_cast<bool>(in >> out);
        }

        bool readBool(bool& out)
        {
            int v = 0;
            if (!(in >> v)) return false;
            out = (v != 0);
            return true;
        }

        std::istream& in;
    };
}

namespace TimelineAsset
{
    bool save(
        const std::filesystem::path& filePath,
        float duration,
        const std::vector<TimelineComponent::Track>& tracks,
        const std::vector<TimelineComponent::Clip>& clips,
        const std::vector<TimelineComponent::Signal>& signals)
    {
        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            LOG_ERROR("[TimelineAsset] Failed to open for write: %s", filePath.string().c_str());
            return false;
        }

        out << "TIMELINE " << kTimelineAssetVersion << "\n";
        out << "DURATION " << duration << "\n";
        out << "TRACK_COUNT " << tracks.size() << "\n";
        for (const auto& track : tracks)
        {
            out << "TRACK "
                << std::quoted(track.name) << " "
                << (track.muted ? 1 : 0) << " "
                << (track.solo ? 1 : 0) << " "
                << track.weight << "\n";
        }

        out << "CLIP_COUNT " << clips.size() << "\n";
        for (const auto& clip : clips)
        {
            out << "CLIP "
                << (clip.enabled ? 1 : 0) << " "
                << std::quoted(clip.animationName) << " "
                << clip.track << " "
                << clip.weight << " "
                << clip.startTime << " "
                << clip.duration << " "
                << clip.clipInTime << " "
                << clip.speed << " "
                << (clip.loop ? 1 : 0) << " "
                << clip.blendIn << " "
                << clip.blendOut << " "
                << static_cast<int>(clip.blendInCurve) << " "
                << static_cast<int>(clip.blendOutCurve) << "\n";
        }

        out << "SIGNAL_COUNT " << signals.size() << "\n";
        for (const auto& signal : signals)
        {
            out << "SIGNAL "
                << (signal.enabled ? 1 : 0) << " "
                << std::quoted(signal.eventName) << " "
                << signal.time << "\n";
        }

        return true;
    }

    bool load(
        const std::filesystem::path& filePath,
        float& outDuration,
        std::vector<TimelineComponent::Track>& outTracks,
        std::vector<TimelineComponent::Clip>& outClips,
        std::vector<TimelineComponent::Signal>& outSignals)
    {
        std::ifstream in(filePath, std::ios::binary);
        if (!in)
        {
            LOG_ERROR("[TimelineAsset] Failed to open for read: %s", filePath.string().c_str());
            return false;
        }

        TokenReader reader(in);
        if (!reader.expect("TIMELINE"))
        {
            LOG_ERROR("[TimelineAsset] Invalid header: %s", filePath.string().c_str());
            return false;
        }

        int version = 0;
        if (!reader.readInt(version) || version != kTimelineAssetVersion)
        {
            LOG_ERROR("[TimelineAsset] Unsupported version (%d): %s", version, filePath.string().c_str());
            return false;
        }

        if (!reader.expect("DURATION") || !reader.readFloat(outDuration))
        {
            LOG_ERROR("[TimelineAsset] Failed to read duration: %s", filePath.string().c_str());
            return false;
        }

        if (!reader.expect("TRACK_COUNT"))
        {
            LOG_ERROR("[TimelineAsset] Missing TRACK_COUNT: %s", filePath.string().c_str());
            return false;
        }

        int trackCount = 0;
        if (!reader.readInt(trackCount) || trackCount < 0)
        {
            LOG_ERROR("[TimelineAsset] Invalid track count: %s", filePath.string().c_str());
            return false;
        }

        outTracks.clear();
        outTracks.reserve(static_cast<size_t>(trackCount));
        for (int i = 0; i < trackCount; ++i)
        {
            if (!reader.expect("TRACK")) return false;

            TimelineComponent::Track track;
            if (!reader.readString(track.name)) return false;
            if (!reader.readBool(track.muted)) return false;
            if (!reader.readBool(track.solo)) return false;
            if (!reader.readFloat(track.weight)) return false;
            outTracks.push_back(std::move(track));
        }

        if (!reader.expect("CLIP_COUNT"))
        {
            LOG_ERROR("[TimelineAsset] Missing CLIP_COUNT: %s", filePath.string().c_str());
            return false;
        }

        int clipCount = 0;
        if (!reader.readInt(clipCount) || clipCount < 0)
        {
            LOG_ERROR("[TimelineAsset] Invalid clip count: %s", filePath.string().c_str());
            return false;
        }

        outClips.clear();
        outClips.reserve(static_cast<size_t>(clipCount));
        for (int i = 0; i < clipCount; ++i)
        {
            if (!reader.expect("CLIP")) return false;

            TimelineComponent::Clip clip;
            int blendInCurve = 0;
            int blendOutCurve = 0;
            if (!reader.readBool(clip.enabled)) return false;
            if (!reader.readString(clip.animationName)) return false;
            if (!reader.readInt(clip.track)) return false;
            if (!reader.readFloat(clip.weight)) return false;
            if (!reader.readFloat(clip.startTime)) return false;
            if (!reader.readFloat(clip.duration)) return false;
            if (!reader.readFloat(clip.clipInTime)) return false;
            if (!reader.readFloat(clip.speed)) return false;
            if (!reader.readBool(clip.loop)) return false;
            if (!reader.readFloat(clip.blendIn)) return false;
            if (!reader.readFloat(clip.blendOut)) return false;
            if (!reader.readInt(blendInCurve)) return false;
            if (!reader.readInt(blendOutCurve)) return false;
            clip.blendInCurve = static_cast<TimelineComponent::BlendCurve>(std::clamp(blendInCurve, 0, 4));
            clip.blendOutCurve = static_cast<TimelineComponent::BlendCurve>(std::clamp(blendOutCurve, 0, 4));
            outClips.push_back(std::move(clip));
        }

        if (!reader.expect("SIGNAL_COUNT"))
        {
            LOG_ERROR("[TimelineAsset] Missing SIGNAL_COUNT: %s", filePath.string().c_str());
            return false;
        }

        int signalCount = 0;
        if (!reader.readInt(signalCount) || signalCount < 0)
        {
            LOG_ERROR("[TimelineAsset] Invalid signal count: %s", filePath.string().c_str());
            return false;
        }

        outSignals.clear();
        outSignals.reserve(static_cast<size_t>(signalCount));
        for (int i = 0; i < signalCount; ++i)
        {
            if (!reader.expect("SIGNAL")) return false;

            TimelineComponent::Signal signal;
            if (!reader.readBool(signal.enabled)) return false;
            if (!reader.readString(signal.eventName)) return false;
            if (!reader.readFloat(signal.time)) return false;
            outSignals.push_back(std::move(signal));
        }

        return true;
    }
}
