#pragma once

#include "Model\Model.h"

//! ozz-animation を既存 Model 表現へ接続するランタイム
class OzzAnimationRuntime
{
public:
    OzzAnimationRuntime();
    ~OzzAnimationRuntime();

    OzzAnimationRuntime(const OzzAnimationRuntime&) = delete;
    OzzAnimationRuntime& operator=(const OzzAnimationRuntime&) = delete;

    //! FBX から読み込まれたスケルトンとクリップを ozz 形式へ変換
    bool initialize(const ModelResource::Model& modelData);

    //! 圧縮済み ozz クリップをサンプリング
    bool sample(int animationIndex, float timeSeconds, std::vector<Model::Bone>& outPose) const;

    //! 2つのローカルポーズを ozz BlendingJob で合成
    bool blend(const std::vector<Model::Bone>& first,
        const std::vector<Model::Bone>& second,
        float weight,
        std::vector<Model::Bone>& outPose) const;

    //! ジョイントマスク付きの Override / Additive レイヤー合成
    bool blendLayer(const std::vector<Model::Bone>& basePose,
        const std::vector<Model::Bone>& layerPose,
        float weight,
        const std::vector<int>& engineBoneMask,
        bool additive,
        bool additiveScale,
        bool additiveTranslation,
        std::vector<Model::Bone>& outPose) const;

    //! ozz IKTwoBoneJob で3ジョイントチェーンを解決
    bool solveTwoBoneIK(Model& model,
        int upperIndex,
        int lowerIndex,
        int endIndex,
        const Vector3& targetWorld,
        const Vector3& poleVectorWorld,
        const Matrix& modelWorld,
        float weight,
        bool* reached = nullptr) const;

    //! ozz IKAimJob で親から子へ連なるジョイントをターゲットへ向ける
    bool solveAimIK(Model& model,
        const std::vector<int>& jointChain,
        const std::vector<Vector3>& localForwards,
        const std::vector<Vector3>& localUps,
        const std::vector<float>& jointWeights,
        const Vector3& targetWorld,
        const Vector3& poleVectorWorld,
        const Matrix& modelWorld,
        float weight,
        float maxCorrectionRadians,
        bool* reached = nullptr) const;

    bool isReady() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};