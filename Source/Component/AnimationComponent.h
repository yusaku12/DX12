#pragma once

#include "Component\Component.h"
#include "Model\Model.h"
#include "Animation\AnimationStateMachine.h"

//=====================================================
//! アニメーションコンポーネント
//! Unity の Animator / UE の AnimInstance 相当
//! 機能:
//!  - ステートマシンによるパラメータ駆動の自動遷移
//!  - Any State 遷移
//!  - インデックス / 名前指定でダイレクト再生
//!  - クロスフェード（前後アニメーションの補間遷移）
//!  - ループ / ワンショット / PingPong
//!  - 再生速度制御
//!  - アニメーションイベント（特定時刻のコールバック発火）
//!  - レイヤーブレンド（上半身/下半身分離など）
//!  - 再生完了コールバック
//!  - シーケンサーによるデバッグ UI
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

    //! アニメーション追加読み込み（FBX ファイルから）
    void addAnimation(const char* filename);

    //! ステートマシンへのアクセス
    AnimationStateMachine& getStateMachine() { return m_stateMachine; }
    const AnimationStateMachine& getStateMachine() const { return m_stateMachine; }

    //! ステートマシンモードの有効/無効
    //! 無効時はダイレクト再生 API を使用する
    void setStateMachineEnabled(bool enabled) { m_useStateMachine = enabled; }
    bool isStateMachineEnabled() const { return m_useStateMachine; }

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
    void  setSpeed(float speed) { m_speed = speed; }

    //! 再生中か
    bool isPlaying() const;

    //! 一時停止中か
    bool isPaused() const { return m_paused; }

    //! 再生完了（ワンショット再生で最後まで到達）したか
    bool isFinished() const { return m_finished; }

    //! クロスフェード中か
    bool isFading() const;

    //! 現在の再生時間（秒）
    float getCurrentTime() const;

    //! 正規化された再生位置（0.0～1.0）
    float getNormalizedTime() const;

    //! 現在のアニメーションインデックス（再生中でない場合は -1）
    int getCurrentAnimationIndex() const;

    //! 現在のアニメーション名（再生中でない場合は空文字列）
    const std::string& getCurrentAnimationName() const;

    //! アニメーション名からインデックスを検索（見つからない場合 -1）
    int findAnimationIndex(const std::string& name) const;

    //! 再生完了時のコールバックを設定（ワンショット再生でのみ発火）
    void setOnFinished(OnFinishedCallback callback) { m_onFinished = std::move(callback); }

private:

    //! 指定アニメーションのキーフレーム補間結果をボーン配列に書き込む
    void evaluateAnimation(int animIndex, float time,
        std::vector<Model::Bone>& bones) const;

    //! ボーン配列同士をブレンド: out = lerp(a, b, t)
    static void blendBones(const std::vector<Model::Bone>& a,
        const std::vector<Model::Bone>& b,
        float t,
        std::vector<Model::Bone>& out);

    //! 指定アニメーションのサンプリング間隔を取得
    float getSamplingTime(int animIndex) const;

    void drawSequencer();
    void drawDebugInfo();

    Model* m_model = nullptr;

    //! ステートマシン
    AnimationStateMachine m_stateMachine;
    bool m_useStateMachine = false;

    //! ダイレクト再生: 現在のアニメーション
    int   m_animationIndex = -1;
    float m_currentTime = 0.0f;
    float m_speed = 1.0f;
    bool  m_loop = true;
    bool  m_playing = false;
    bool  m_paused = false;
    bool  m_finished = false;

    //! ダイレクト再生: クロスフェード
    int   m_prevAnimIndex = -1;
    float m_prevTime = 0.0f;
    float m_fadeDuration = 0.0f;
    float m_fadeElapsed = 0.0f;
    bool  m_fading = false;

    //! コールバック
    OnFinishedCallback m_onFinished;

    //! シーケンサー用
    int32_t m_seqCurrentFrame = 0;

    //! 空文字列（参照戻り用）
    static inline const std::string s_emptyString;
};