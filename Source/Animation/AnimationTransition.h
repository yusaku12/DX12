#pragma once

#include "AnimationCommon.h"
#include <string>
#include <vector>

//=====================================================
//! 遷移条件（1つ分）
//=====================================================
struct TransitionCondition
{
    std::string paramName;            //!< 参照するパラメータ名
    CompareOp   op = CompareOp::Equal;
    float       threshold = 0.0f;     //!< 比較閾値（Bool なら 1.0 = true）
};

//=====================================================
//! アニメーション遷移定義
//! Unity の AnimatorStateTransition 相当
//=====================================================
struct AnimationTransition
{
    std::string destStateName;                   //!< 遷移先ステート名
    float fadeDuration = 0.2f;                   //!< クロスフェード時間（秒）
    float exitTime = 0.0f;                       //!< 遷移開始の正規化時刻（0 = 即座）
    bool  hasExitTime = false;                   //!< exitTime を使うか
    bool  interruptible = true;                  //!< 遷移中に他の遷移で中断可能か
    std::vector<TransitionCondition> conditions; //!< すべて満たされたとき遷移
};