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
    float          weight = 1.0f;
    LayerBlendMode blendMode = LayerBlendMode::Override;

    //! ボーンマスク（対象ボーンインデックス一覧）
    //! 空の場合は全ボーン対象
    std::vector<int> boneMask;

    //! このレイヤーで動作するステートマシンのインデックス
    //! （AnimationStateMachine 内で管理）
    int stateMachineIndex = 0;
};