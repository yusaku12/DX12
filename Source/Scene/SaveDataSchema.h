#pragma once

#include <cstdint>

namespace SaveDataSchema
{
    struct VersionCheckResult
    {
        bool supported = false;
        bool needsResave = false;
    };

    constexpr uint32_t kSceneCurrentVersion = 3;
    constexpr uint32_t kSceneMinimumSupportedVersion = 1;

    constexpr uint32_t kPrefabCurrentVersion = 1;
    constexpr uint32_t kPrefabMinimumSupportedVersion = 1;

    inline VersionCheckResult checkSceneVersion(uint32_t version)
    {
        if (version > kSceneCurrentVersion)
        {
            return { false, false };
        }

        if (version < kSceneMinimumSupportedVersion)
        {
            return { false, false };
        }

        return { true, version != kSceneCurrentVersion };
    }

    inline VersionCheckResult checkPrefabVersion(uint32_t version)
    {
        if (version > kPrefabCurrentVersion)
        {
            return { false, false };
        }

        if (version < kPrefabMinimumSupportedVersion)
        {
            return { false, false };
        }

        return { true, version != kPrefabCurrentVersion };
    }
}
