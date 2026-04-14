#pragma once

#include "AnimationCommon.h"
#include <string>

//=====================================================
//! アニメーションパラメータ（Animator のコントロール変数）
//! Unity の AnimatorControllerParameter 相当
//=====================================================
struct AnimationParameter
{
    std::string   name;
    AnimParamType type = AnimParamType::Bool;
    float floatValue = 0.0f;
    int   intValue = 0;
    bool  boolValue = false;

    //! Trigger は一度消費したら自動で false に戻る
    bool  triggerFired = false;
};