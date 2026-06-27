#pragma once

#include "AnimationStateMachine.h"

namespace AnimatorControllerAsset
{
    bool save(const std::filesystem::path& filePath, const AnimationStateMachine& stateMachine);
    bool load(const std::filesystem::path& filePath, AnimationStateMachine& stateMachine);
}
