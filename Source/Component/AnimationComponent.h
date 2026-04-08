#pragma once

#include "Component\Component.h"
#include "Model\Model.h"

//=====================================================
// アニメーションコンポーネント
//  - 指定アニメーションの再生・停止・ループ
//  - キーフレーム間の線形補間
//=====================================================
class AnimationComponent : public Component
{
public:

    AnimationComponent() = default;
    ~AnimationComponent() override;

    //! 初期化（IRenderComponent からモデルを取得）
    void awake() override;

    //! 毎フレーム更新
    void update() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! アニメーション追加読み込み（FBXファイルから）
    void addAnimation(const char* filename);

    //! アニメーション再生（インデックス指定）
    void play(int animationIndex, bool loop = true);

    //! 停止
    void stop();

    //! 再生中か
    bool isPlaying() const { return m_playing; }

    //! 再生終了したか（ループ時は常に false）
    bool isFinished() const { return m_finished; }

    //! 現在の再生時間
    float getCurrentTime() const { return m_currentTime; }

private:

    //! キーフレーム補間してボーンに適用
    void evaluate();

    Model* m_model = nullptr;
    int    m_animationIndex = -1;
    float  m_currentTime = 0.0f;
    bool   m_loop = true;
    bool   m_playing = false;
    bool   m_finished = false;
};
