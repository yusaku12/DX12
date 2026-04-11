#pragma once

#include "Component\Component.h"
#include "Model\Model.h"

//=====================================================
// アニメーションコンポーネント
// 機能:
//  - インデックス / 名前指定で再生
//  - ループ / ワンショット
//  - クロスフェード（前アニメーションと現アニメーションの補間遷移）
//  - 再生速度制御
//  - 再生完了コールバック
//  - シーケンサーによるデバッグUI
//=====================================================
class AnimationComponent : public Component
{
public:

    //! 再生完了時コールバック型
    using OnFinishedCallback = std::function<void()>;

    AnimationComponent() = default;
    ~AnimationComponent() override = default;

    //! 初期化
    void awake() override;

    //! 毎フレーム更新
    void update() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! アニメーション追加読み込み（FBXファイルから）
    void addAnimation(const char* filename);

    //! インデックス指定で再生
    void play(int animationIndex, bool loop = true, float speed = 1.0f);

    //! 名前指定で再生
    void play(const std::string& animationName, bool loop = true, float speed = 1.0f);

    //! クロスフェード付きで遷移（インデックス指定）
    void crossFade(int animationIndex, float fadeDuration, bool loop = true, float speed = 1.0f);

    //! クロスフェード付きで遷移（名前指定）
    void crossFade(const std::string& animationName, float fadeDuration, bool loop = true, float speed = 1.0f);

    //! 停止
    void stop();

    //! 一時停止 / 再開
    void pause() { m_paused = true; }
    void resume() { m_paused = false; }

    //! 再生速度
    float getSpeed() const { return m_speed; }
    void setSpeed(float speed) { m_speed = speed; }

    //! 再生中か
    bool isPlaying() const { return m_playing && !m_paused; }

    //! 一時停止中か
    bool isPaused() const { return m_paused; }

    //! 再生完了（ワンショット再生で最後まで到達）したか
    bool isFinished() const { return m_finished; }

    //! クロスフェード中か
    bool isFading() const { return m_fading; }

    //! 現在の再生時間（秒）
    float getCurrentTime() const { return m_currentTime; }

    //! 正規化された再生位置（0.0～1.0）
    float getNormalizedTime() const;

    //! 現在のアニメーションインデックス（再生中でない場合は -1）
    int getCurrentAnimationIndex() const { return m_animationIndex; }

    //! 現在のアニメーション名（再生中でない場合は空文字列）
    const std::string& getCurrentAnimationName() const;

    //! アニメーション名からインデックスを検索（見つからない場合 -1）
    int findAnimationIndex(const std::string& name) const;

    //! 再生完了時のコールバックを設定（ワンショット再生でのみ発火）
    void setOnFinished(OnFinishedCallback callback) { m_onFinished = std::move(callback); }

private:

    //! 指定アニメーションのキーフレーム補間結果をボーン配列に書き込む
    void evaluateAnimation(int animIndex, float time, std::vector<Model::Bone>& bones) const;

    //! ボーン配列同士をブレンド: out = lerp(a, b, t)
    static void blendBones(
        const std::vector<Model::Bone>& a,
        const std::vector<Model::Bone>& b,
        float t,
        std::vector<Model::Bone>& out);

    //! 指定アニメーションのサンプリング間隔を取得
    float getSamplingTime(int animIndex) const;

    void drawSequencer();

    void drawDebugInfo();

    Model* m_model = nullptr;

    //! 現在のアニメーション
    int   m_animationIndex = -1;
    float m_currentTime = 0.0f;
    float m_speed = 1.0f;
    bool  m_loop = true;
    bool  m_playing = false;
    bool  m_paused = false;
    bool  m_finished = false;

    //! クロスフェード
    int   m_prevAnimIndex = -1;  //!< フェード元アニメーション
    float m_prevTime = 0.0f;     //!< フェード元の再生時間（フェード開始時点で固定）
    float m_fadeDuration = 0.0f; //!< フェード所要時間
    float m_fadeElapsed = 0.0f;  //!< フェード経過時間
    bool  m_fading = false;

    //! コールバック
    OnFinishedCallback m_onFinished;

    //! シーケンサー用
    int32_t m_seqCurrentFrame = 0;

    //! 空文字列（参照戻り用）
    static inline const std::string s_emptyString;
};