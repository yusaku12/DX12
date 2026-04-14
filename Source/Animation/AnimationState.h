#pragma once

#include "AnimationCommon.h"
#include "AnimationEvent.h"
#include "AnimationTransition.h"

//=====================================================
//! アニメーションステート
//! ステートマシンの各ノードに対応
//! Unity の AnimatorState / UE の AnimState 相当
//=====================================================
class AnimationState
{
public:

    AnimationState() = default;
    explicit AnimationState(const std::string& name, int animIndex = -1)
        : m_name(name), m_animationIndex(animIndex) {
    }

    //! ステート名
    const std::string& getName() const { return m_name; }

    //! アニメーションクリップインデックス
    int  getAnimationIndex() const { return m_animationIndex; }
    void setAnimationIndex(int idx) { m_animationIndex = idx; }

    //! ループモード
    LoopMode getLoopMode() const { return m_loopMode; }
    void setLoopMode(LoopMode mode) { m_loopMode = mode; }

    //! 再生速度倍率
    float getSpeed() const { return m_speed; }
    void  setSpeed(float s) { m_speed = s; }

    //! 遷移を追加
    void addTransition(const AnimationTransition& transition)
    {
        m_transitions.push_back(transition);
    }

    //! 遷移一覧
    const std::vector<AnimationTransition>& getTransitions() const { return m_transitions; }
    std::vector<AnimationTransition>& getTransitions() { return m_transitions; }

    //! イベントを追加
    void addEvent(const AnimationEvent& evt)
    {
        m_events.push_back(evt);
    }

    //! イベント一覧
    std::vector<AnimationEvent>& getEvents() { return m_events; }
    const std::vector<AnimationEvent>& getEvents() const { return m_events; }

    //! イベント発火済みフラグをリセット
    void resetEvents()
    {
        for (auto& e : m_events) e.fired = false;
    }

private:

    std::string m_name;
    int         m_animationIndex = -1;
    LoopMode    m_loopMode = LoopMode::Loop;
    float       m_speed = 1.0f;

    std::vector<AnimationTransition> m_transitions;
    std::vector<AnimationEvent>      m_events;
};