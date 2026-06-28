#pragma once

#include "AnimationState.h"
#include "AnimationParameter.h"
#include "AnimationLayer.h"
#include "Model\Model.h"

//=====================================================
//! アニメーションステートマシン
//! Unity の AnimatorController / UE の AnimBlueprint 相当
//! 機能:
//!  - パラメータベースの自動遷移
//!  - クロスフェード付きステート遷移
//!  - レイヤーブレンド（上半身/下半身分離など）
//!  - アニメーションイベント（特定時刻のコールバック発火）
//!  - PingPong / Loop / Once ループモード
//=====================================================
class AnimationStateMachine
{
public:

    AnimationStateMachine() = default;

    //! 初期化
    void initialize(Model* model);

    //! 毎フレーム更新
    void update(float deltaTime);

    //! ステートを追加（戻り値: 追加されたステートへのポインタ）
    AnimationState* addState(const std::string& name, int animIndex = -1);

    //! ステートを削除（関連遷移も自動クリーンアップ）
    bool removeState(const std::string& name);

    //! ステートを名前で検索
    AnimationState* findState(const std::string& name);
    const AnimationState* findState(const std::string& name) const;

    //! デフォルト（エントリー）ステートを設定
    void setDefaultState(const std::string& name);

    //! 現在のコントローラーデータをクリア（モデル参照は維持）
    void clearController();

    //! デフォルトステート名
    const std::string& getDefaultStateName() const;

    //! 現在のステート名
    const std::string& getCurrentStateName() const;

    //! 現在のステート
    const AnimationState* getCurrentState() const;

    //! パラメータを追加
    void addParameter(const std::string& name, AnimParamType type);

    //! パラメータ設定
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);
    void setBool(const std::string& name, bool value);
    void setTrigger(const std::string& name);

    //! パラメータ取得
    float getFloat(const std::string& name) const;
    int   getInt(const std::string& name) const;
    bool  getBool(const std::string& name) const;

    //! パラメータ一覧
    std::unordered_map<std::string, AnimationParameter>& getParameters() { return m_parameters; }
    const std::unordered_map<std::string, AnimationParameter>& getParameters() const { return m_parameters; }

    //! ステート一覧
    std::vector<std::unique_ptr<AnimationState>>& getStates() { return m_states; }
    const std::vector<std::unique_ptr<AnimationState>>& getStates() const { return m_states; }

    //! レイヤー一覧
    std::vector<AnimationLayer>& getLayers() { return m_layers; }
    const std::vector<AnimationLayer>& getLayers() const { return m_layers; }

    //! レイヤーを追加（戻り値: 追加後インデックス）
    size_t addLayer(const AnimationLayer& layer);

    //! レイヤー削除（0=Base Layer は削除不可）
    bool removeLayer(size_t index);

    //! レイヤーの参照取得
    AnimationLayer* getLayer(size_t index);
    const AnimationLayer* getLayer(size_t index) const;

    //! レイヤーのモーションソース設定
    bool setLayerAnimation(size_t index, int animationIndex, float speed = 1.0f, bool loop = true);
    bool setLayerUseCurrentStatePose(size_t index, bool useCurrentStatePose);

    //! 現在の正規化再生時間（0.0～1.0）
    float getNormalizedTime() const { return m_normalizedTime; }

    //! クロスフェード中か
    bool isFading() const { return m_fading; }

    //! 再生中か
    bool isPlaying() const { return m_currentState != nullptr; }

    //! 指定ステートに即時遷移（条件なし）
    void forceTransition(const std::string& stateName, float fadeDuration = 0.2f);

    //! 遷移を重複無しで追加（成功時 true）
    bool addTransitionUnique(const std::string& fromStateName,
        const std::string& toStateName,
        float fadeDuration = 0.2f);

    //! 現在ステートの再生長（秒）
    float getCurrentStateLength() const;

private:

    //! 遷移条件を評価
    bool evaluateConditions(const std::vector<TransitionCondition>& conditions) const;

    //! 遷移を実行
    void executeTransition(AnimationState* destState, float fadeDuration);

    //! アニメーションのキーフレーム補間結果をボーンに書き込む
    void evaluateAnimation(int animIndex, float time,
        std::vector<Model::Bone>& bones) const;

    //! ステート（クリップ or ブレンドツリー）の結果をボーンに書き込む
    void evaluateStatePose(const AnimationState& state, float time,
        std::vector<Model::Bone>& bones) const;

    //! ボーン配列同士をブレンド: out = lerp(a, b, t)
    static void blendBones(const std::vector<Model::Bone>& a,
        const std::vector<Model::Bone>& b,
        float t,
        std::vector<Model::Bone>& out);

    //! ボーン配列同士をマスク付きブレンド
    void blendBonesWithMask(const std::vector<Model::Bone>& src,
        const std::vector<Model::Bone>& layer,
        float weight,
        const std::vector<int>& mask,
        LayerBlendMode mode,
        bool additiveAffectScale,
        bool additiveAffectTranslation,
        std::vector<Model::Bone>& out);

    //! 指定アニメーションの総再生時間を取得
    float getAnimationLength(int animIndex) const;

    //! ステートの総再生時間（ブレンドツリー時は子の最大長）
    float getStateLength(const AnimationState& state) const;

    //! イベント発火チェック
    void fireEvents(AnimationState* state, float prevNorm, float currNorm);

    //! Trigger パラメータを消費（遷移実行後にリセット）
    void consumeTriggers(const std::vector<TransitionCondition>& conditions);

    //! パラメータ検索
    AnimationParameter* findParam(const std::string& name);
    const AnimationParameter* findParam(const std::string& name) const;

    Model* m_model = nullptr;

    //! ステート一覧
    std::vector<std::unique_ptr<AnimationState>> m_states;
    AnimationState* m_currentState = nullptr;
    AnimationState* m_defaultState = nullptr;

    //! パラメータ一覧
    std::unordered_map<std::string, AnimationParameter> m_parameters;

    //! レイヤー
    std::vector<AnimationLayer> m_layers;

    //! 再生状態
    float m_currentTime = 0.0f;
    float m_normalizedTime = 0.0f;
    bool  m_pingPongReverse = false; //!< PingPong 時の往復フラグ

    //! クロスフェード
    AnimationState* m_prevState = nullptr;
    float m_prevTime = 0.0f;
    float m_fadeDuration = 0.0f;
    float m_fadeElapsed = 0.0f;
    bool  m_fading = false;

    //! 空文字列（参照戻り用）
    static inline const std::string s_emptyString;
};