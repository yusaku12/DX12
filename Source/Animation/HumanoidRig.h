#pragma once

#include <string>
#include <string_view>
#include <cctype>
#include <cstdint>
#include <array>

//=====================================================
//! Humanoid bone identification helper
//! Lightweight mapping close to Unity Humanoid major bones
//=====================================================
enum class HumanBodyBone : uint8_t
{
    Hips = 0,
    Spine,
    Chest,
    UpperChest,
    Neck,
    Head,

    LeftShoulder,
    LeftUpperArm,
    LeftLowerArm,
    LeftHand,

    RightShoulder,
    RightUpperArm,
    RightLowerArm,
    RightHand,

    LeftUpperLeg,
    LeftLowerLeg,
    LeftFoot,
    LeftToes,

    RightUpperLeg,
    RightLowerLeg,
    RightFoot,
    RightToes,

    Last,
    Invalid = 255,
};

class HumanoidRig
{
public:

    static constexpr size_t BoneCount = static_cast<size_t>(HumanBodyBone::Last);

    static std::string normalizeBoneName(std::string_view name)
    {
        std::string out;
        out.reserve(name.size());

        for (char c : name)
        {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc) != 0)
            {
                out.push_back(static_cast<char>(std::tolower(uc)));
            }
        }
        return out;
    }

    static HumanBodyBone classify(std::string_view rawName)
    {
        const std::string name = normalizeBoneName(rawName);
        if (name.empty())
        {
            return HumanBodyBone::Invalid;
        }

        const bool isLeft = containsAny(name, s_leftTokens);
        const bool isRight = containsAny(name, s_rightTokens);

        if (contains(name, "hips") || contains(name, "pelvis"))
        {
            return HumanBodyBone::Hips;
        }
        if (contains(name, "upperchest"))
        {
            return HumanBodyBone::UpperChest;
        }
        if (contains(name, "chest"))
        {
            return HumanBodyBone::Chest;
        }
        if (contains(name, "spine") || contains(name, "abdomen"))
        {
            return HumanBodyBone::Spine;
        }
        if (contains(name, "neck"))
        {
            return HumanBodyBone::Neck;
        }
        if (contains(name, "head"))
        {
            return HumanBodyBone::Head;
        }

        if (isLeft)
        {
            if (contains(name, "shoulder") || contains(name, "clavicle")) return HumanBodyBone::LeftShoulder;
            if (contains(name, "lowerarm") || contains(name, "forearm") || contains(name, "loarm") || contains(name, "elbow")) return HumanBodyBone::LeftLowerArm;
            if (contains(name, "upperarm") || contains(name, "uparm") || contains(name, "bicep")) return HumanBodyBone::LeftUpperArm;
            if (contains(name, "hand") || contains(name, "wrist")) return HumanBodyBone::LeftHand;

            if (contains(name, "upperleg") || contains(name, "thigh") || contains(name, "upleg")) return HumanBodyBone::LeftUpperLeg;
            if (contains(name, "lowerleg") || contains(name, "calf") || contains(name, "knee") || contains(name, "loleg")) return HumanBodyBone::LeftLowerLeg;
            if (contains(name, "foot") || contains(name, "ankle")) return HumanBodyBone::LeftFoot;
            if (contains(name, "toe")) return HumanBodyBone::LeftToes;
        }

        if (isRight)
        {
            if (contains(name, "shoulder") || contains(name, "clavicle")) return HumanBodyBone::RightShoulder;
            if (contains(name, "lowerarm") || contains(name, "forearm") || contains(name, "loarm") || contains(name, "elbow")) return HumanBodyBone::RightLowerArm;
            if (contains(name, "upperarm") || contains(name, "uparm") || contains(name, "bicep")) return HumanBodyBone::RightUpperArm;
            if (contains(name, "hand") || contains(name, "wrist")) return HumanBodyBone::RightHand;

            if (contains(name, "upperleg") || contains(name, "thigh") || contains(name, "upleg")) return HumanBodyBone::RightUpperLeg;
            if (contains(name, "lowerleg") || contains(name, "calf") || contains(name, "knee") || contains(name, "loleg")) return HumanBodyBone::RightLowerLeg;
            if (contains(name, "foot") || contains(name, "ankle")) return HumanBodyBone::RightFoot;
            if (contains(name, "toe")) return HumanBodyBone::RightToes;
        }

        return HumanBodyBone::Invalid;
    }

private:

    static bool contains(const std::string& text, std::string_view token)
    {
        return text.find(token) != std::string::npos;
    }

public:

    static bool isLikelyHelperBone(std::string_view rawName)
    {
        const std::string name = normalizeBoneName(rawName);
        if (name.empty()) return true;

        return contains(name, "twist") ||
            contains(name, "roll") ||
            contains(name, "helper") ||
            contains(name, "ik") ||
            contains(name, "pole") ||
            contains(name, "end") ||
            contains(name, "nub") ||
            contains(name, "socket") ||
            contains(name, "weapon");
    }

    static bool containsAny(const std::string& text, const std::array<std::string_view, 3>& tokens)
    {
        for (std::string_view token : tokens)
        {
            if (token.empty()) continue;
            if (contains(text, token)) return true;
        }
        return false;
    }

    static inline const std::array<std::string_view, 3> s_leftTokens =
    {
        "left", "lft", "lf"
    };

    static inline const std::array<std::string_view, 3> s_rightTokens =
    {
        "right", "rgt", "rt"
    };
};
