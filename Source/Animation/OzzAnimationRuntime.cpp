#include "pch.h"
#include "OzzAnimationRuntime.h"

#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/ik_aim_job.h>
#include <ozz/animation/runtime/ik_two_bone_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/maths/simd_quaternion.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/span.h>

namespace
{
    bool requiresQuaternionRepair(const Vector4& rotation)
    {
        const double lengthSquared =
            static_cast<double>(rotation.x) * rotation.x +
            static_cast<double>(rotation.y) * rotation.y +
            static_cast<double>(rotation.z) * rotation.z +
            static_cast<double>(rotation.w) * rotation.w;
        return !std::isfinite(lengthSquared) ||
            std::abs(lengthSquared - 1.0) >= ozz::math::kNormalizationToleranceEstSq;
    }

    Vector4 repairQuaternion(const Vector4& rotation)
    {
        const double lengthSquared =
            static_cast<double>(rotation.x) * rotation.x +
            static_cast<double>(rotation.y) * rotation.y +
            static_cast<double>(rotation.z) * rotation.z +
            static_cast<double>(rotation.w) * rotation.w;
        if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12)
        {
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        }

        const float inverseLength = static_cast<float>(1.0 / std::sqrt(lengthSquared));
        return rotation * inverseLength;
    }

    ozz::math::SimdQuaternion repairQuaternionIfNeeded(const ozz::math::SimdQuaternion& rotation)
    {
        XMFLOAT4A values;
        ozz::math::StorePtr(rotation.xyzw, &values.x);
        Vector4 scalar(values.x, values.y, values.z, values.w);
        if (!requiresQuaternionRepair(scalar))
        {
            return rotation;
        }

        scalar = repairQuaternion(scalar);
        values = { scalar.x, scalar.y, scalar.z, scalar.w };
        ozz::math::SimdQuaternion repaired;
        repaired.xyzw = ozz::math::simd_float4::LoadPtr(&values.x);
        return repaired;
    }

    ozz::math::Transform toOzzTransform(const Vector3& scale, const Vector4& rotation, const Vector3& translation)
    {
        ozz::math::Transform transform;
        transform.translation = { translation.x, translation.y, translation.z };
        transform.rotation = { rotation.x, rotation.y, rotation.z, rotation.w };
        transform.scale = { scale.x, scale.y, scale.z };
        return transform;
    }

}

struct OzzAnimationRuntime::Impl
{
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton;
    std::vector<ozz::unique_ptr<ozz::animation::Animation>> animations;
    std::vector<std::unique_ptr<ozz::animation::SamplingJob::Context>> contexts;
    std::vector<int> ozzToEngine;
    std::vector<int> engineToOzz;
    bool reportedAimQuaternionRepair = false;

    bool buildSkeleton(const ModelResource::Model& modelData)
    {
        if (modelData.bones.empty() || modelData.bones.size() > ozz::animation::Skeleton::kMaxJoints)
        {
            return false;
        }

        std::vector<std::vector<int>> children(modelData.bones.size());
        std::vector<int> roots;
        for (int index = 0; index < static_cast<int>(modelData.bones.size()); ++index)
        {
            const int parent = modelData.bones[index].parentIndex;
            if (parent >= 0 && parent < static_cast<int>(modelData.bones.size()))
            {
                children[parent].push_back(index);
            }
            else
            {
                roots.push_back(index);
            }
        }

        ozz::animation::offline::RawSkeleton rawSkeleton;
        ozzToEngine.clear();
        std::function<void(int, ozz::animation::offline::RawSkeleton::Joint::Children&)> appendJoint;
        appendJoint = [&](int engineIndex, ozz::animation::offline::RawSkeleton::Joint::Children& destination)
            {
                destination.resize(destination.size() + 1);
                auto& joint = destination.back();
                const auto& source = modelData.bones[engineIndex];
                joint.name = source.name.c_str();
                joint.transform = toOzzTransform(source.scale, source.rotate, source.translate);
                ozzToEngine.push_back(engineIndex);
                for (int child : children[engineIndex])
                {
                    appendJoint(child, joint.children);
                }
            };

        for (int root : roots)
        {
            appendJoint(root, rawSkeleton.roots);
        }

        ozz::animation::offline::SkeletonBuilder builder;
        skeleton = builder(rawSkeleton);
        if (!skeleton || skeleton->num_joints() != static_cast<int>(modelData.bones.size()))
        {
            return false;
        }

        engineToOzz.assign(modelData.bones.size(), -1);
        for (int ozzIndex = 0; ozzIndex < static_cast<int>(ozzToEngine.size()); ++ozzIndex)
        {
            engineToOzz[ozzToEngine[ozzIndex]] = ozzIndex;
        }
        return true;
    }

    bool buildAnimations(const ModelResource::Model& modelData)
    {
        animations.clear();
        contexts.clear();
        animations.reserve(modelData.animations.size());
        contexts.reserve(modelData.animations.size());

        ozz::animation::offline::AnimationBuilder builder;
        for (const auto& sourceAnimation : modelData.animations)
        {
            ozz::animation::offline::RawAnimation rawAnimation;
            rawAnimation.name = sourceAnimation.name.c_str();
            rawAnimation.duration = std::max(sourceAnimation.secondsLength, 0.0001f);
            rawAnimation.tracks.resize(modelData.bones.size());

            for (int ozzIndex = 0; ozzIndex < static_cast<int>(ozzToEngine.size()); ++ozzIndex)
            {
                const int engineIndex = ozzToEngine[ozzIndex];
                auto& track = rawAnimation.tracks[ozzIndex];
                for (const auto& frame : sourceAnimation.keyframes)
                {
                    if (engineIndex >= static_cast<int>(frame.nodeKeys.size()))
                    {
                        continue;
                    }

                    const auto& key = frame.nodeKeys[engineIndex];
                    const float time = std::clamp(frame.seconds, 0.0f, rawAnimation.duration);
                    track.translations.push_back({ time, { key.translate.x, key.translate.y, key.translate.z } });
                    track.rotations.push_back({ time, { key.rotate.x, key.rotate.y, key.rotate.z, key.rotate.w } });
                    track.scales.push_back({ time, { key.scale.x, key.scale.y, key.scale.z } });
                }
            }

            auto animation = builder(rawAnimation);
            if (!animation)
            {
                LOG_ERROR("[OzzAnimationRuntime] Failed to build animation: %s", sourceAnimation.name.c_str());
                return false;
            }

            contexts.push_back(std::make_unique<ozz::animation::SamplingJob::Context>(animation->num_tracks()));
            animations.push_back(std::move(animation));
        }
        return true;
    }

    void poseToSoa(const std::vector<Model::Bone>& pose, std::vector<ozz::math::SoaTransform>& soaPose) const
    {
        soaPose.assign(skeleton->joint_rest_poses().begin(), skeleton->joint_rest_poses().end());
        for (int ozzIndex = 0; ozzIndex < static_cast<int>(ozzToEngine.size()); ++ozzIndex)
        {
            const int engineIndex = ozzToEngine[ozzIndex];
            if (engineIndex >= static_cast<int>(pose.size())) continue;

            auto& soa = soaPose[ozzIndex / 4];
            const int lane = ozzIndex & 3;
            const auto& bone = pose[engineIndex];
            soa.translation.x = ozz::math::SetI(soa.translation.x, ozz::math::simd_float4::Load1(bone.translate.x), lane);
            soa.translation.y = ozz::math::SetI(soa.translation.y, ozz::math::simd_float4::Load1(bone.translate.y), lane);
            soa.translation.z = ozz::math::SetI(soa.translation.z, ozz::math::simd_float4::Load1(bone.translate.z), lane);
            soa.rotation.x = ozz::math::SetI(soa.rotation.x, ozz::math::simd_float4::Load1(bone.rotate.x), lane);
            soa.rotation.y = ozz::math::SetI(soa.rotation.y, ozz::math::simd_float4::Load1(bone.rotate.y), lane);
            soa.rotation.z = ozz::math::SetI(soa.rotation.z, ozz::math::simd_float4::Load1(bone.rotate.z), lane);
            soa.rotation.w = ozz::math::SetI(soa.rotation.w, ozz::math::simd_float4::Load1(bone.rotate.w), lane);
            soa.scale.x = ozz::math::SetI(soa.scale.x, ozz::math::simd_float4::Load1(bone.scale.x), lane);
            soa.scale.y = ozz::math::SetI(soa.scale.y, ozz::math::simd_float4::Load1(bone.scale.y), lane);
            soa.scale.z = ozz::math::SetI(soa.scale.z, ozz::math::simd_float4::Load1(bone.scale.z), lane);
        }
    }

    void soaToPose(const std::vector<ozz::math::SoaTransform>& soaPose, std::vector<Model::Bone>& pose) const
    {
        for (int ozzIndex = 0; ozzIndex < static_cast<int>(ozzToEngine.size()); ++ozzIndex)
        {
            const int engineIndex = ozzToEngine[ozzIndex];
            if (engineIndex >= static_cast<int>(pose.size())) continue;

            const auto& soa = soaPose[ozzIndex / 4];
            const int lane = ozzIndex & 3;
            auto laneValue = [lane](ozz::math::SimdFloat4 value)
                {
                    alignas(16) float values[4];
                    ozz::math::StorePtr(value, values);
                    return values[lane];
                };

            auto& bone = pose[engineIndex];
            bone.translate = { laneValue(soa.translation.x), laneValue(soa.translation.y), laneValue(soa.translation.z) };
            bone.rotate = { laneValue(soa.rotation.x), laneValue(soa.rotation.y), laneValue(soa.rotation.z), laneValue(soa.rotation.w) };
            bone.scale = { laneValue(soa.scale.x), laneValue(soa.scale.y), laneValue(soa.scale.z) };
        }
    }
};

OzzAnimationRuntime::OzzAnimationRuntime()
    : m_impl(std::make_unique<Impl>())
{
}

OzzAnimationRuntime::~OzzAnimationRuntime() = default;

bool OzzAnimationRuntime::initialize(const ModelResource::Model& modelData)
{
    m_impl->reportedAimQuaternionRepair = false;
    if (!m_impl->buildSkeleton(modelData) || !m_impl->buildAnimations(modelData))
    {
        m_impl->skeleton.reset();
        m_impl->animations.clear();
        m_impl->contexts.clear();
        return false;
    }

    LOG_INFO("[OzzAnimationRuntime] Built %zu joints and %zu clips.", modelData.bones.size(), modelData.animations.size());
    return true;
}

bool OzzAnimationRuntime::sample(int animationIndex, float timeSeconds, std::vector<Model::Bone>& outPose) const
{
    if (!isReady() || animationIndex < 0 || animationIndex >= static_cast<int>(m_impl->animations.size()))
    {
        return false;
    }

    const auto& animation = m_impl->animations[animationIndex];
    std::vector<ozz::math::SoaTransform> locals(m_impl->skeleton->num_soa_joints());
    const float ratio = animation->duration() > 0.0f
        ? std::clamp(timeSeconds / animation->duration(), 0.0f, 1.0f)
        : 0.0f;

    ozz::animation::SamplingJob job;
    job.animation = animation.get();
    job.context = m_impl->contexts[animationIndex].get();
    job.ratio = ratio;
    job.output = ozz::make_span(locals);
    if (!job.Run())
    {
        return false;
    }

    m_impl->soaToPose(locals, outPose);
    return true;
}

bool OzzAnimationRuntime::blend(const std::vector<Model::Bone>& first,
    const std::vector<Model::Bone>& second,
    float weight,
    std::vector<Model::Bone>& outPose) const
{
    if (!isReady()) return false;

    std::vector<ozz::math::SoaTransform> firstSoa;
    std::vector<ozz::math::SoaTransform> secondSoa;
    std::vector<ozz::math::SoaTransform> output(m_impl->skeleton->num_soa_joints());
    m_impl->poseToSoa(first, firstSoa);
    m_impl->poseToSoa(second, secondSoa);

    std::array<ozz::animation::BlendingJob::Layer, 2> layers;
    layers[0].weight = 1.0f - std::clamp(weight, 0.0f, 1.0f);
    layers[0].transform = ozz::make_span(firstSoa);
    layers[1].weight = std::clamp(weight, 0.0f, 1.0f);
    layers[1].transform = ozz::make_span(secondSoa);

    ozz::animation::BlendingJob job;
    job.layers = ozz::make_span(layers);
    job.rest_pose = m_impl->skeleton->joint_rest_poses();
    job.output = ozz::make_span(output);
    if (!job.Run()) return false;

    m_impl->soaToPose(output, outPose);
    return true;
}

bool OzzAnimationRuntime::blendLayer(const std::vector<Model::Bone>& basePose,
    const std::vector<Model::Bone>& layerPose,
    float weight,
    const std::vector<int>& engineBoneMask,
    bool additive,
    bool additiveScale,
    bool additiveTranslation,
    std::vector<Model::Bone>& outPose) const
{
    if (!isReady()) return false;

    std::vector<Model::Bone> preparedLayer = layerPose;
    if (additive)
    {
        const auto restPose = m_impl->skeleton->joint_rest_poses();
        std::vector<Model::Bone> engineRest = basePose;
        m_impl->soaToPose(std::vector<ozz::math::SoaTransform>(restPose.begin(), restPose.end()), engineRest);
        const size_t count = std::min(preparedLayer.size(), engineRest.size());
        for (size_t index = 0; index < count; ++index)
        {
            const auto& reference = engineRest[index];
            auto& layer = preparedLayer[index];

            if (additiveScale)
            {
                layer.scale.x = reference.scale.x != 0.0f ? layer.scale.x / reference.scale.x : 1.0f;
                layer.scale.y = reference.scale.y != 0.0f ? layer.scale.y / reference.scale.y : 1.0f;
                layer.scale.z = reference.scale.z != 0.0f ? layer.scale.z / reference.scale.z : 1.0f;
            }
            else
            {
                layer.scale = Vector3::One;
            }

            const XMVECTOR referenceRotation = XMQuaternionNormalize(XMLoadFloat4(&reference.rotate));
            const XMVECTOR layerRotation = XMQuaternionNormalize(XMLoadFloat4(&layer.rotate));
            XMStoreFloat4(&layer.rotate, XMQuaternionMultiply(XMQuaternionInverse(referenceRotation), layerRotation));
            layer.translate = additiveTranslation ? layer.translate - reference.translate : Vector3::Zero;
        }
    }

    std::vector<ozz::math::SoaTransform> baseSoa;
    std::vector<ozz::math::SoaTransform> layerSoa;
    std::vector<ozz::math::SoaTransform> output(m_impl->skeleton->num_soa_joints());
    m_impl->poseToSoa(basePose, baseSoa);
    m_impl->poseToSoa(preparedLayer, layerSoa);

    std::vector<ozz::math::SimdFloat4> jointWeights(m_impl->skeleton->num_soa_joints(), ozz::math::simd_float4::zero());
    for (int engineIndex : engineBoneMask)
    {
        if (engineIndex < 0 || engineIndex >= static_cast<int>(m_impl->engineToOzz.size())) continue;
        const int ozzIndex = m_impl->engineToOzz[engineIndex];
        if (ozzIndex < 0) continue;
        auto& soaWeight = jointWeights[ozzIndex / 4];
        soaWeight = ozz::math::SetI(soaWeight, ozz::math::simd_float4::one(), ozzIndex & 3);
    }

    ozz::animation::BlendingJob::Layer baseLayer;
    baseLayer.weight = 1.0f;
    baseLayer.transform = ozz::make_span(baseSoa);
    ozz::animation::BlendingJob::Layer maskedLayer;
    maskedLayer.weight = std::clamp(weight, 0.0f, 1.0f);
    maskedLayer.transform = ozz::make_span(layerSoa);
    maskedLayer.joint_weights = ozz::make_span(jointWeights);

    std::array<ozz::animation::BlendingJob::Layer, 2> overrideLayers = { baseLayer, maskedLayer };
    std::array<ozz::animation::BlendingJob::Layer, 1> additiveLayers = { maskedLayer };
    ozz::animation::BlendingJob job;
    if (additive)
    {
        job.layers = ozz::span<const ozz::animation::BlendingJob::Layer>(&baseLayer, 1);
        job.additive_layers = ozz::make_span(additiveLayers);
    }
    else
    {
        job.layers = ozz::make_span(overrideLayers);
    }
    job.rest_pose = m_impl->skeleton->joint_rest_poses();
    job.output = ozz::make_span(output);
    if (!job.Run()) return false;

    m_impl->soaToPose(output, outPose);
    return true;
}

bool OzzAnimationRuntime::solveTwoBoneIK(Model& model,
    int upperIndex,
    int lowerIndex,
    int endIndex,
    const Vector3& targetWorld,
    const Vector3& poleVectorWorld,
    const Matrix& modelWorld,
    float weight,
    bool* reached) const
{
    if (!isReady() || upperIndex < 0 || lowerIndex < 0 || endIndex < 0) return false;
    if (upperIndex >= static_cast<int>(m_impl->engineToOzz.size()) ||
        lowerIndex >= static_cast<int>(m_impl->engineToOzz.size()) ||
        endIndex >= static_cast<int>(m_impl->engineToOzz.size())) return false;

    auto& bones = model.getMutableBone();
    std::vector<ozz::math::SoaTransform> locals;
    m_impl->poseToSoa(bones, locals);
    std::vector<ozz::math::Float4x4> models(m_impl->skeleton->num_joints());

    ozz::animation::LocalToModelJob localToModel;
    localToModel.skeleton = m_impl->skeleton.get();
    localToModel.input = ozz::make_span(locals);
    localToModel.output = ozz::make_span(models);
    if (!localToModel.Run()) return false;

    const int upperOzz = m_impl->engineToOzz[upperIndex];
    const int lowerOzz = m_impl->engineToOzz[lowerIndex];
    const int endOzz = m_impl->engineToOzz[endIndex];
    if (upperOzz < 0 || lowerOzz < 0 || endOzz < 0) return false;

    const Matrix worldToModel = modelWorld.Invert();
    const Vector3 targetModel = Vector3::Transform(targetWorld, worldToModel);
    Vector3 poleModel = Vector3::TransformNormal(poleVectorWorld, worldToModel);
    if (poleModel.LengthSquared() <= 0.000001f) poleModel = Vector3::Forward;
    poleModel.Normalize();

    ozz::animation::IKTwoBoneJob job;
    job.target = ozz::math::simd_float4::Load3PtrU(&targetModel.x);
    job.pole_vector = ozz::math::simd_float4::Load3PtrU(&poleModel.x);
    job.weight = std::clamp(weight, 0.0f, 1.0f);
    job.start_joint = &models[upperOzz];
    job.mid_joint = &models[lowerOzz];
    job.end_joint = &models[endOzz];
    ozz::math::SimdQuaternion upperCorrection;
    ozz::math::SimdQuaternion lowerCorrection;
    job.start_joint_correction = &upperCorrection;
    job.mid_joint_correction = &lowerCorrection;
    job.reached = reached;
    if (!job.Run()) return false;

    auto applyCorrection = [&](int ozzIndex, const ozz::math::SimdQuaternion& correction)
        {
            auto& soa = locals[ozzIndex / 4];
            ozz::math::SimdQuaternion rotations[4];
            ozz::math::Transpose4x4(&soa.rotation.x, &rotations->xyzw);
            rotations[ozzIndex & 3] = rotations[ozzIndex & 3] * correction;
            ozz::math::Transpose4x4(&rotations->xyzw, &soa.rotation.x);
        };
    applyCorrection(upperOzz, upperCorrection);
    applyCorrection(lowerOzz, lowerCorrection);
    m_impl->soaToPose(locals, bones);
    return true;
}

bool OzzAnimationRuntime::solveAimIK(Model& model,
    const std::vector<int>& jointChain,
    const std::vector<Vector3>& localForwards,
    const std::vector<Vector3>& localUps,
    const std::vector<float>& jointWeights,
    const Vector3& targetWorld,
    const Vector3& poleVectorWorld,
    const Matrix& modelWorld,
    float weight,
    float maxCorrectionRadians,
    bool* reached) const
{
    if (reached) *reached = false;
    if (!isReady() || jointChain.empty() ||
        jointChain.size() != localForwards.size() ||
        jointChain.size() != localUps.size() ||
        jointChain.size() != jointWeights.size())
    {
        return false;
    }

    auto& bones = model.getMutableBone();
    std::vector<ozz::math::SoaTransform> locals;
    m_impl->poseToSoa(bones, locals);

    int repairedRotationCount = 0;
    std::string firstRepairedBone;
    for (int ozzIndex = 0; ozzIndex < static_cast<int>(m_impl->ozzToEngine.size()); ++ozzIndex)
    {
        const int engineIndex = m_impl->ozzToEngine[ozzIndex];
        if (engineIndex < 0 || engineIndex >= static_cast<int>(bones.size())) continue;

        const Vector4 rotation = bones[engineIndex].rotate;
        if (!requiresQuaternionRepair(rotation)) continue;

        const Vector4 repaired = repairQuaternion(rotation);
        auto& soa = locals[ozzIndex / 4];
        const int lane = ozzIndex & 3;
        soa.rotation.x = ozz::math::SetI(soa.rotation.x, ozz::math::simd_float4::Load1(repaired.x), lane);
        soa.rotation.y = ozz::math::SetI(soa.rotation.y, ozz::math::simd_float4::Load1(repaired.y), lane);
        soa.rotation.z = ozz::math::SetI(soa.rotation.z, ozz::math::simd_float4::Load1(repaired.z), lane);
        soa.rotation.w = ozz::math::SetI(soa.rotation.w, ozz::math::simd_float4::Load1(repaired.w), lane);
        if (firstRepairedBone.empty()) firstRepairedBone = bones[engineIndex].name;
        ++repairedRotationCount;
    }
    if (repairedRotationCount > 0 && !m_impl->reportedAimQuaternionRepair)
    {
        LOG_WARN("[OzzAnimationRuntime] Repaired %d invalid Aim IK quaternion(s). First bone: %s",
            repairedRotationCount, firstRepairedBone.c_str());
        m_impl->reportedAimQuaternionRepair = true;
    }

    std::vector<ozz::math::Float4x4> models(m_impl->skeleton->num_joints());

    ozz::animation::LocalToModelJob localToModel;
    localToModel.skeleton = m_impl->skeleton.get();
    localToModel.input = ozz::make_span(locals);
    localToModel.output = ozz::make_span(models);
    if (!localToModel.Run()) return false;

    const Matrix worldToModel = modelWorld.Invert();
    const Vector3 targetModel = Vector3::Transform(targetWorld, worldToModel);
    Vector3 poleModel = Vector3::TransformNormal(poleVectorWorld, worldToModel);
    if (poleModel.LengthSquared() <= 0.000001f) poleModel = Vector3::Up;
    poleModel.Normalize();

    bool processedJoint = false;
    bool allReached = true;
    for (size_t chainIndex = 0; chainIndex < jointChain.size(); ++chainIndex)
    {
        const int engineIndex = jointChain[chainIndex];
        if (engineIndex < 0 || engineIndex >= static_cast<int>(m_impl->engineToOzz.size())) continue;
        const int ozzIndex = m_impl->engineToOzz[engineIndex];
        if (ozzIndex < 0 || ozzIndex >= static_cast<int>(models.size())) continue;

        Vector3 forward = localForwards[chainIndex];
        Vector3 up = localUps[chainIndex];
        if (forward.LengthSquared() <= 0.000001f || up.LengthSquared() <= 0.000001f) continue;
        forward.Normalize();
        up -= forward * up.Dot(forward);
        if (up.LengthSquared() <= 0.000001f) continue;
        up.Normalize();

        ozz::animation::IKAimJob aim;
        aim.target = ozz::math::simd_float4::Load3PtrU(&targetModel.x);
        aim.forward = ozz::math::simd_float4::Load3PtrU(&forward.x);
        aim.up = ozz::math::simd_float4::Load3PtrU(&up.x);
        aim.pole_vector = ozz::math::simd_float4::Load3PtrU(&poleModel.x);
        aim.weight = std::clamp(weight * jointWeights[chainIndex], 0.0f, 1.0f);
        aim.joint = &models[ozzIndex];

        ozz::math::SimdQuaternion correction;
        bool jointReached = false;
        aim.joint_correction = &correction;
        aim.reached = &jointReached;
        if (!aim.Run()) return false;
        processedJoint = true;
        allReached &= jointReached;

        if (maxCorrectionRadians > 0.0f)
        {
            XMFLOAT4A values;
            ozz::math::StorePtr(correction.xyzw, &values.x);
            XMVECTOR quaternion = XMQuaternionNormalize(XMLoadFloat4A(&values));
            const float angle = 2.0f * std::acos(std::clamp(std::abs(XMVectorGetW(quaternion)), 0.0f, 1.0f));
            if (angle > maxCorrectionRadians)
            {
                allReached = false;
                quaternion = XMQuaternionSlerp(XMQuaternionIdentity(), quaternion, maxCorrectionRadians / angle);
                XMStoreFloat4A(&values, quaternion);
                correction.xyzw = ozz::math::simd_float4::LoadPtr(&values.x);
            }
        }

        auto& soa = locals[ozzIndex / 4];
        ozz::math::SimdQuaternion rotations[4];
        ozz::math::Transpose4x4(&soa.rotation.x, &rotations->xyzw);
        rotations[ozzIndex & 3] = repairQuaternionIfNeeded(rotations[ozzIndex & 3] * correction);
        ozz::math::Transpose4x4(&rotations->xyzw, &soa.rotation.x);

        localToModel.from = ozzIndex;
        if (!localToModel.Run()) return false;
    }

    m_impl->soaToPose(locals, bones);
    if (reached) *reached = processedJoint && allReached;
    return true;
}

bool OzzAnimationRuntime::isReady() const
{
    return m_impl && m_impl->skeleton != nullptr;
}