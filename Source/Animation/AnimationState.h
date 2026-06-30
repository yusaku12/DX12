#pragma once

#include "AnimationCommon.h"
#include "AnimationEvent.h"
#include "AnimationTransition.h"
#include "BlendTree.h"

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

    //! ノードエディタ上の位置
    const Vector2& getNodePosition() const { return m_nodePosition; }
    void setNodePosition(const Vector2& pos) { m_nodePosition = pos; }

    //! ブレンドツリー
    bool hasBlendTree() const { return m_blendTree != nullptr; }
    BlendTreeData& createBlendTree(BlendTreeType type = BlendTreeType::Blend1D)
    {
        if (!m_blendTree)
        {
            m_blendTree = DXMem::makeUnique<BlendTreeData>();
        }
        m_blendTree->type = type;
        return *m_blendTree;
    }
    void clearBlendTree() { m_blendTree.reset(); }
    BlendTreeData* getBlendTree() { return m_blendTree.get(); }
    const BlendTreeData* getBlendTree() const { return m_blendTree.get(); }

    //! デバッグ・プレビュー用の代表アニメーション
    int getPreviewAnimationIndex() const
    {
        if (m_blendTree && !m_blendTree->children.empty())
        {
            return m_blendTree->children.front().animationIndex;
        }
        return m_animationIndex;
    }

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
    Vector2     m_nodePosition = { 0, 0 };

    std::vector<AnimationTransition> m_transitions;
    std::vector<AnimationEvent>      m_events;
    std::unique_ptr<BlendTreeData>   m_blendTree;
};