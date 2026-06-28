#pragma once

#include "AnimationCommon.h"
#include <string>
#include <vector>

//=====================================================
//! アニメーションレイヤー
//! 複数のアニメーションを部位ごとにブレンドする仕組み
//! Unity の AnimatorControllerLayer / UE の AnimLayer 相当
//=====================================================
struct AnimationLayer
{
    std::string    name = "Base Layer";
    bool           enabled = true;
    float          weight = 1.0f;
    LayerBlendMode blendMode = LayerBlendMode::Override;

    //! true の場合は現在ステートのポーズを再利用
    //! false の場合は layerAnimationIndex を独立再生
    bool           useCurrentStatePose = true;
    int            layerAnimationIndex = -1;
    float          layerTime = 0.0f;
    float          layerSpeed = 1.0f;
    bool           layerLoop = true;

    //! Additive 適用成分
    bool           additiveAffectScale = false;
    bool           additiveAffectTranslation = false;

    //! ボーンマスク（対象ボーンインデックス一覧）
    //! 空の場合は全ボーン対象
    std::vector<int> boneMask;

    //! このレイヤーで動作するステートマシンのインデックス
    //! （AnimationStateMachine 内で管理）
    int stateMachineIndex = 0;
};