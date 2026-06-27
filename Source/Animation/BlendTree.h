#pragma once

#include "AnimationCommon.h"
#include "Math\SimpleMath.h"

// Blend tree child motion.
struct BlendTreeChild
{
    int animationIndex = -1;
    float threshold = 0.0f;      // For 1D blend.
    Vector2 position = { 0, 0 }; // For 2D blend.
    float timeScale = 1.0f;
};

// Blend tree data (1D / 2D freeform).
struct BlendTreeData
{
    BlendTreeType type = BlendTreeType::Blend1D;
    std::string parameterX;
    std::string parameterY;
    std::vector<BlendTreeChild> children;
};
