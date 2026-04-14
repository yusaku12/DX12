#pragma once
#include <string>
#include <functional>

//=====================================================
//! アニメーションイベント
//! アニメーション再生中の特定時刻で発火するコールバック
//! Unity の AnimationEvent / UE の AnimNotify 相当
//=====================================================
struct AnimationEvent
{
    std::string name;               //!< イベント名
    float normalizedTime = 0.0f;    //!< 発火タイミング（0.0～1.0）
    std::function<void()> callback; //!< コールバック
    bool fired = false;             //! 発火済みフラグ（ループ時にリセット）
};