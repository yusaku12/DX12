#pragma once

#include "Component\Component.h"
#include "Model\Model.h"
#include "Animation\AnimationStateMachine.h"
#include "Animation\HumanoidRig.h"
#include "Animation\OzzAnimationRuntime.h"
#include <array>

class GameObject;

//=====================================================
//! アニメーションコンポーネント
//! Unity の Animator / UE の AnimInstance 相当
//! 機能:
//!  - ステートマシンによるパラメータ駆動の自動遷移
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

    AnimationComponent();
    ~AnimationComponent() override = default;

    //! 初期化
    void awake() override;

    //! 毎フレーム更新
    void update() override;

    //! アニメーションと外部 Pose 適用後に LookAt を解決
    void lateUpdate() override;

    //! 破棄時クリーンアップ
    void onDestroy() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! ステートマシンモードの有効/無効
    //! 無効時はダイレクト再生 API を使用する
    void setStateMachineEnabled(bool enabled) { m_useStateMachine = enabled; }
    bool isStateMachineEnabled() const { return m_useStateMachine; }

    //! Animator Controller アセット
    const std::string& getControllerAssetPath() const { return m_controllerAssetPath; }
    void setControllerAssetPath(const std::string& path) { m_controllerAssetPath = path; }
    bool saveControllerAsset() const;
    bool loadControllerAsset(const std::string& path);
    bool reloadControllerAsset();

    //! インデックス指定で再生
    void play(int animationIndex, bool loop = true, float speed = 1.0f);

    //! 名前指定で再生
    void play(const std::string& animationName, bool loop = true, float speed = 1.0f);

    //! クロスフェード付きで遷移（インデックス指定）
    void crossFade(int animationIndex, float fadeDuration, bool loop = true, float speed = 1.0f);

    //! クロスフェード付きで遷移（名前指定）
    void crossFade(const std::string& animationName, float fadeDuration, bool loop = true, float speed = 1.0f);

    //! 一時停止 / 再開
    void pause() { m_paused = true; }
    void resume() { m_paused = false; }

    //! 再生速度
    float getSpeed() const { return m_speed; }
    void  setSpeed(float speed) { m_speed = speed; }

    //! 再生中か
    bool isPlaying() const;

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

    //! 読み込まれているアニメーション数
    int getAnimationCount() const;

    //! インデックスからアニメーション名を取得（不正な場合は空文字列）
    std::string getAnimationName(int animationIndex) const;

    //! インデックスからアニメーション長（秒）を取得（不正な場合は 0）
    float getAnimationLength(int animationIndex) const;

    //! 指定時刻のポーズを即時サンプリングして適用する
    //! Timeline などの外部制御向け API（内部再生状態は停止状態に揃える）
    bool sampleAnimation(int animationIndex, float timeSeconds, bool applyRetarget = true);

    //! 指定時刻のポーズを評価して outPose に書き込む（モデルへの適用はしない）
    bool evaluateAnimationPose(int animationIndex, float timeSeconds, std::vector<Model::Bone>& outPose) const;

    //! 外部で合成したポーズをモデルへ適用する
    bool applyPose(const std::vector<Model::Bone>& pose, bool applyRetarget = true);

    //! Humanoid リターゲットの有効/無効
    void setRetargetEnabled(bool enabled) { m_retargetEnabled = enabled; }
    bool isRetargetEnabled() const { return m_retargetEnabled; }

    //! リターゲット対象の GameObject 名を指定
    void setRetargetTargetObjectName(const std::string& objectName);
    const std::string& getRetargetTargetObjectName() const { return m_retargetTargetObjectName; }

    //! 現在のリターゲット先モデル
    Model* getRetargetTargetModel() const { return m_retargetModel; }

    //! 自身が現在駆動しているモデル
    Model* getModel() const { return m_model; }

    //! リターゲット先を再解決（成功時 true）
    bool resolveRetargetTarget();

    //! 上半身 Additive レイヤーを構成（戻り値: レイヤー index、失敗時 -1）
    int setupUpperBodyAdditiveLayer(int animationIndex, float weight = 1.0f, float speed = 1.0f, bool loop = true);

    //! 下半身 Additive レイヤーを構成（戻り値: レイヤー index、失敗時 -1）
    int setupLowerBodyAdditiveLayer(int animationIndex, float weight = 1.0f, float speed = 1.0f, bool loop = true);

    //! 足 IK
    void setFootIKEnabled(bool left, bool enabled);
    void setFootIKTarget(bool left, const Vector3& worldTarget, float weight = 1.0f);

    //! 腕 IK
    void setArmIKEnabled(bool left, bool enabled);
    void setArmIKTarget(bool left, const Vector3& worldTarget, float weight = 1.0f);

    //! Head / Neck / Chest の LookAt
    void setLookAtEnabled(bool enabled);
    bool isLookAtEnabled() const { return m_lookAt.enabled; }
    void setLookAtTarget(const Vector3& worldTarget, float weight = 1.0f);
    const Vector3& getLookAtTarget() const { return m_lookAt.targetWorld; }
    float getLookAtWeight() const { return m_lookAt.weight; }
    void setLookAtWeight(float weight) { m_lookAt.weight = std::clamp(weight, 0.0f, 1.0f); }
    float getLookAtFollowSharpness() const { return m_lookAt.followSharpness; }
    void setLookAtFollowSharpness(float sharpness) { m_lookAt.followSharpness = std::max(sharpness, 0.0f); }
    float getLookAtMaxCorrectionDegrees() const { return m_lookAt.maxCorrectionDegrees; }
    void setLookAtMaxCorrectionDegrees(float degrees) { m_lookAt.maxCorrectionDegrees = std::clamp(degrees, 0.0f, 180.0f); }
    float getDebugMouseLookAtDistance() const { return m_lookAt.debugTargetDistance; }
    void setDebugMouseLookAtDistance(float distance) { m_lookAt.debugTargetDistance = std::max(distance, 0.5f); }

    //! デバッグ時に Scene ビューのマウス位置を注視する
    void setDebugMouseLookAtEnabled(bool enabled)
    {
        m_lookAt.debugMouseTarget = enabled;
        m_lookAt.hasSmoothedTarget = false;
        if (enabled) m_lookAt.enabled = true;
    }
    bool isDebugMouseLookAtEnabled() const { return m_lookAt.debugMouseTarget; }

    //! 他コンポーネントから外部ポーズを書き込まれる間は自前更新を停止
    void setExternalRetargetOverride(bool enabled) { m_externalRetargetOverride = enabled; }
    bool isExternalRetargetOverridden() const { return m_externalRetargetOverride; }

private:

    //! アニメーション追加読み込み（FBX ファイルから）
    void addAnimation(const char* filename);

    //! 再生停止
    void stop();

    //! 指定アニメーションのキーフレーム補間結果をボーン配列に書き込む
    void evaluateAnimation(int animIndex, float time,
        std::vector<Model::Bone>& bones) const;

    //! ボーン配列同士をブレンド: out = lerp(a, b, t)
    void blendBones(const std::vector<Model::Bone>& a,
        const std::vector<Model::Bone>& b,
        float t,
        std::vector<Model::Bone>& out) const;

    //! 指定アニメーションのサンプリング間隔を取得
    float getSamplingTime(int animIndex) const;

    //! Humanoid リターゲット対象を解決する
    void resolveRetargetTargetInternal();

    //! Humanoid ボーン対応表を再構築する
    void rebuildRetargetMap();

    //! 現在の source ポーズを target モデルへ反映する
    void applyRetargetFromCurrentPose();

    //! 自分自身の humanoid マップを再構築する
    void rebuildSelfHumanoidMap();

    //! 上半身/下半身のボーンマスクを収集する
    std::vector<int> collectHumanoidBoneMask(bool upperBody) const;

    //! LookAt チェーンと各ボーンのローカル軸を構築
    void rebuildLookAtChain();

    //! Scene ビューのマウス位置から安定したワールドターゲットを取得
    bool resolveDebugMouseLookAtTarget(Vector3& outTarget) const;

    //! ozz Aim IK を適用
    void applyLookAt();

    //! IK 解決を適用する
    void applyIK();

    //! ozz 2 ボーン IK を適用
    void solveTwoBoneIK(int upperIndex, int lowerIndex, int endIndex,
        const Vector3& targetWorld,
        float weight,
        int iterationCount,
        float maxStepDegrees);

    //! モデルの基準ワールド行列を取得
    Matrix getModelWorldMatrix() const;

    void drawSequencer();
    void drawDebugInfo();
    void drawAnimatorWindow();
    void rebuildAnimatorGraph();
    void drawAnimatorStateInspector(AnimationState* state);
    void drawSelectedTransitionInspector();

    Model* m_model = nullptr;
    OzzAnimationRuntime m_ozzRuntime;
    OzzAnimationRuntime m_retargetOzzRuntime;

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

    //! Animator ウィンドウ
    bool m_showAnimatorWindow = true;
    bool m_animatorGraphDirty = true;
    std::string m_selectedStateName;
    std::string m_selectedTransitionFromStateName;
    int m_selectedTransitionIndex = -1;
    bool m_dragCreatingTransition = false;
    std::string m_dragFromStateName;
    std::string m_contextMenuStateName;
    Vector2 m_animatorCanvasPan = { 0.0f, 0.0f };
    std::string m_controllerAssetPath;
    std::string m_newStateName = "NewState";
    std::string m_newParamName = "Speed";
    int m_newParamType = 0;

    //! Humanoid リターゲット
    bool m_retargetEnabled = false;
    std::string m_retargetTargetObjectName;
    GameObject* m_retargetTargetObject = nullptr;
    Model* m_retargetModel = nullptr;
    bool m_retargetMapDirty = true;
    int m_retargetMappedBoneCount = 0;
    float m_retargetRootTranslationScale = 1.0f;
    std::array<int, HumanoidRig::BoneCount> m_retargetSourceHumanToBone{};
    std::array<int, HumanoidRig::BoneCount> m_retargetTargetHumanToBone{};
    std::array<ModelResource::NodeKeyData, HumanoidRig::BoneCount> m_retargetSourceBindPose{};
    std::array<ModelResource::NodeKeyData, HumanoidRig::BoneCount> m_retargetTargetBindPose{};
    std::array<float, HumanoidRig::BoneCount> m_retargetTranslationScale{};
    std::array<Vector4, HumanoidRig::BoneCount> m_retargetAxisAlign{};
    std::array<bool, HumanoidRig::BoneCount> m_retargetAxisAlignValid{};
    bool m_externalRetargetOverride = false;

    //! 自身の humanoid マップ
    std::array<int, HumanoidRig::BoneCount> m_selfHumanToBone{};
    bool m_selfHumanoidMapDirty = true;

    struct LookAtGoal
    {
        bool enabled = false;
        bool debugMouseTarget = false;
        bool hasTarget = false;
        bool hasSmoothedTarget = false;
        bool reached = false;
        float weight = 1.0f;
        float followSharpness = 10.0f;
        float maxCorrectionDegrees = 70.0f;
        float debugTargetDistance = 6.0f;
        Vector3 targetWorld = Vector3::Zero;
        Vector3 smoothedTargetWorld = Vector3::Zero;
    };

    LookAtGoal m_lookAt;
    std::vector<int> m_lookAtChain;
    std::vector<Vector3> m_lookAtLocalForwards;
    std::vector<Vector3> m_lookAtLocalUps;
    std::vector<float> m_lookAtJointWeights;
    std::vector<Vector4> m_lookAtBaseRotations;
    std::vector<Vector4> m_lookAtAppliedRotations;
    bool m_hasLookAtAppliedPose = false;
    bool m_lookAtChainDirty = true;

    struct IKGoal
    {
        bool enabled = false;
        bool hasTarget = false;
        bool hasSmoothedTarget = false;
        float weight = 1.0f;
        float followSharpness = 14.0f;
        float maxStepDegrees = 20.0f;
        Vector3 targetWorld = Vector3::Zero;
        Vector3 smoothedTargetWorld = Vector3::Zero;
    };

    IKGoal m_leftFootIK;
    IKGoal m_rightFootIK;
    IKGoal m_leftArmIK;
    IKGoal m_rightArmIK;

    //! 空文字列（参照戻り用）
    static inline const std::string s_emptyString;
};