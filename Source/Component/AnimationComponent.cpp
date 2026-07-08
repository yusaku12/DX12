#include "pch.h"
#include "AnimationComponent.h"
#include "Animation\AnimatorControllerAsset.h"
#include "FbxRenderComponent.h"
#include "Model\FBXLoad.h"
#include "GameObject\GameObject.h"
#include "GameObject\GameObjectRegistry.h"
#include "TransformComponent.h"
#include "imgui_neo_sequencer.h"
#include "System\TimeManager.h"
#include <DirectXMath.h>
#include <unordered_set>

using namespace DirectX;

namespace
{
    constexpr float kAxisEpsilon = 1e-6f;
    constexpr int kIkDefaultIterations = 8;

    Vector3 safeNormalize(const Vector3& v, const Vector3& fallback)
    {
        if (v.LengthSquared() <= kAxisEpsilon)
        {
            return fallback;
        }
        Vector3 n = v;
        n.Normalize();
        return n;
    }

    Vector3 orthogonalFallback(const Vector3& forward)
    {
        Vector3 up = Vector3::Up;
        if (std::abs(forward.Dot(up)) > 0.95f)
        {
            up = Vector3::Right;
        }

        up -= forward * up.Dot(forward);
        return safeNormalize(up, Vector3::Forward);
    }

    bool estimateBoneAxes(const Model& model, int boneIndex, Vector3& outForward, Vector3& outUp)
    {
        const auto& bones = model.getBone();
        if (boneIndex < 0 || boneIndex >= static_cast<int>(bones.size()))
        {
            return false;
        }

        const auto& bone = bones[boneIndex];

        Vector3 avgChildDir = Vector3::Zero;
        std::vector<Vector3> childDirs;
        childDirs.reserve(bone.children.size());
        for (const Model::Bone* child : bone.children)
        {
            if (!child) continue;

            Vector3 d = child->translate;
            if (d.LengthSquared() <= kAxisEpsilon) continue;

            d.Normalize();
            childDirs.push_back(d);
            avgChildDir += d;
        }

        Vector3 forward = Vector3::Zero;
        if (avgChildDir.LengthSquared() > kAxisEpsilon)
        {
            forward = avgChildDir;
        }
        else if (bone.parent && bone.translate.LengthSquared() > kAxisEpsilon)
        {
            forward = -bone.translate;
        }
        else
        {
            forward = Vector3::Up;
        }
        forward = safeNormalize(forward, Vector3::Up);

        Vector3 up = Vector3::Zero;
        if (childDirs.size() >= 2)
        {
            const Vector3& secondary = childDirs[1];
            Vector3 c = forward.Cross(secondary);
            if (c.LengthSquared() > kAxisEpsilon)
            {
                up = c.Cross(forward);
            }
        }

        if (up.LengthSquared() <= kAxisEpsilon)
        {
            up = orthogonalFallback(forward);
        }
        else
        {
            up -= forward * up.Dot(forward);
            up = safeNormalize(up, orthogonalFallback(forward));
        }

        outForward = forward;
        outUp = up;
        return true;
    }

    bool estimateBoneAxesFromBindPose(const std::vector<ModelResource::Bone>& bones,
        int boneIndex,
        Vector3& outForward,
        Vector3& outUp)
    {
        if (boneIndex < 0 || boneIndex >= static_cast<int>(bones.size()))
        {
            return false;
        }

        const auto& bone = bones[boneIndex];

        Vector3 avgChildDir = Vector3::Zero;
        std::vector<Vector3> childDirs;
        for (size_t i = 0; i < bones.size(); ++i)
        {
            if (bones[i].parentIndex != boneIndex) continue;

            Vector3 d = bones[i].translate;
            if (d.LengthSquared() <= kAxisEpsilon) continue;
            d.Normalize();
            childDirs.push_back(d);
            avgChildDir += d;
        }

        Vector3 forward = Vector3::Zero;
        if (avgChildDir.LengthSquared() > kAxisEpsilon)
        {
            forward = avgChildDir;
        }
        else if (bone.parentIndex >= 0 && bone.translate.LengthSquared() > kAxisEpsilon)
        {
            forward = -bone.translate;
        }
        else
        {
            forward = Vector3::Up;
        }
        forward = safeNormalize(forward, Vector3::Up);

        Vector3 up = Vector3::Zero;
        if (childDirs.size() >= 2)
        {
            const Vector3& secondary = childDirs[1];
            Vector3 c = forward.Cross(secondary);
            if (c.LengthSquared() > kAxisEpsilon)
            {
                up = c.Cross(forward);
            }
        }

        if (up.LengthSquared() <= kAxisEpsilon)
        {
            up = orthogonalFallback(forward);
        }
        else
        {
            up -= forward * up.Dot(forward);
            up = safeNormalize(up, orthogonalFallback(forward));
        }

        outForward = forward;
        outUp = up;
        return true;
    }

    XMVECTOR quaternionFromTo(const Vector3& from, const Vector3& to)
    {
        Vector3 f = safeNormalize(from, Vector3::Forward);
        Vector3 t = safeNormalize(to, Vector3::Forward);

        float dot = std::clamp(f.Dot(t), -1.0f, 1.0f);
        if (dot > 0.9999f)
        {
            return XMQuaternionIdentity();
        }

        if (dot < -0.9999f)
        {
            Vector3 axis = f.Cross(Vector3::Right);
            if (axis.LengthSquared() <= kAxisEpsilon)
            {
                axis = f.Cross(Vector3::Up);
            }
            axis = safeNormalize(axis, Vector3::Up);
            return XMQuaternionRotationAxis(XMLoadFloat3(&axis), XM_PI);
        }

        Vector3 axis = f.Cross(t);
        const float s = std::sqrt((1.0f + dot) * 2.0f);
        const float invS = 1.0f / s;

        XMVECTOR q = XMVectorSet(axis.x * invS, axis.y * invS, axis.z * invS, s * 0.5f);
        return XMQuaternionNormalize(q);
    }

    XMVECTOR computeAxisAlignment(const Vector3& srcForward,
        const Vector3& srcUp,
        const Vector3& dstForward,
        const Vector3& dstUp)
    {
        const Vector3 fSrc = safeNormalize(srcForward, Vector3::Forward);
        const Vector3 uSrc = safeNormalize(srcUp, orthogonalFallback(fSrc));
        const Vector3 fDst = safeNormalize(dstForward, Vector3::Forward);
        const Vector3 uDst = safeNormalize(dstUp, orthogonalFallback(fDst));

        XMVECTOR qSwing = quaternionFromTo(fSrc, fDst);

        XMVECTOR vSrcUp = XMLoadFloat3(&uSrc);
        XMVECTOR vRotUp = XMVector3Rotate(vSrcUp, qSwing);
        Vector3 rotUp;
        XMStoreFloat3(&rotUp, vRotUp);

        Vector3 u0 = rotUp - fDst * rotUp.Dot(fDst);
        Vector3 u1 = uDst - fDst * uDst.Dot(fDst);
        if (u0.LengthSquared() <= kAxisEpsilon || u1.LengthSquared() <= kAxisEpsilon)
        {
            return XMQuaternionNormalize(qSwing);
        }

        u0.Normalize();
        u1.Normalize();

        XMVECTOR qTwist = quaternionFromTo(u0, u1);
        XMVECTOR q = XMQuaternionMultiply(qTwist, qSwing);
        return XMQuaternionNormalize(q);
    }

    Vector3 matrixTranslation(const Matrix& m)
    {
        return Vector3(m._41, m._42, m._43);
    }

    Quaternion matrixRotation(const Matrix& m)
    {
        Vector3 scale = Vector3::One;
        Quaternion rotation = Quaternion::Identity;
        Vector3 translation = Vector3::Zero;
        Matrix copy = m;
        if (copy.Decompose(scale, rotation, translation))
        {
            rotation.Normalize();
            return rotation;
        }
        return Quaternion::CreateFromRotationMatrix(m);
    }
}

AnimationComponent::AnimationComponent()
{
    m_selfHumanToBone.fill(-1);
}

void AnimationComponent::awake()
{
    auto* fbxRender = gameObject()->getComponent<FbxRenderComponent>();
    if (fbxRender)
    {
        m_model = fbxRender->getModel();
    }

    // ステートマシンにモデルを渡す
    if (m_model)
    {
        m_stateMachine.initialize(m_model);
        m_selfHumanoidMapDirty = true;
        if (!m_controllerAssetPath.empty())
        {
            reloadControllerAsset();
        }
        m_animatorGraphDirty = true;
    }
}

void AnimationComponent::update()
{
    if (m_externalRetargetOverride) return;

    // Always resolve so disabling retarget can release external override safely.
    resolveRetargetTargetInternal();

    if (!m_model || m_paused) return;

    // ステートマシンモード
    if (m_useStateMachine)
    {
        float dt = TimeManager::Instance().getDeltaTime();
        m_stateMachine.update(dt);

        applyIK();

        if (m_retargetEnabled)
        {
            applyRetargetFromCurrentPose();
        }
        return;
    }

    // ダイレクト再生モード
    if (!m_playing) return;

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (m_animationIndex < 0 || m_animationIndex >= static_cast<int>(animations.size())) return;

    const auto& anim = animations[m_animationIndex];
    float dt = TimeManager::Instance().getDeltaTime() * m_speed;

    // 再生時間を進める
    m_currentTime += dt;

    if (m_currentTime >= anim.secondsLength)
    {
        if (m_loop)
        {
            m_currentTime = std::fmod(m_currentTime, anim.secondsLength);
        }
        else
        {
            m_currentTime = anim.secondsLength;
            m_playing = false;
            m_finished = true;
        }
    }

    // ボーンにポーズを適用
    auto& bones = m_model->getMutableBone();

    if (m_fading)
    {
        // クロスフェード中: prev と current をブレンド
        m_fadeElapsed += dt;
        float t = std::clamp(m_fadeElapsed / m_fadeDuration, 0.0f, 1.0f);

        if (m_prevAnimIndex >= 0 && m_prevAnimIndex < static_cast<int>(animations.size()))
        {
            const auto& prevAnim = animations[m_prevAnimIndex];
            m_prevTime += dt;
            if (m_prevTime >= prevAnim.secondsLength)
            {
                m_prevTime = std::fmod(m_prevTime, prevAnim.secondsLength);
            }
        }

        std::vector<Model::Bone> prevBones = bones;
        evaluateAnimation(m_prevAnimIndex, m_prevTime, prevBones);

        std::vector<Model::Bone> currBones = bones;
        evaluateAnimation(m_animationIndex, m_currentTime, currBones);

        blendBones(prevBones, currBones, t, bones);

        if (m_fadeElapsed >= m_fadeDuration)
        {
            m_fading = false;
            m_prevAnimIndex = -1;
        }
    }
    else
    {
        evaluateAnimation(m_animationIndex, m_currentTime, bones);
    }

    applyIK();

    // 完了コールバック
    if (m_finished && m_onFinished)
    {
        auto callback = std::move(m_onFinished);
        m_onFinished = nullptr;
        callback();
    }

    if (m_retargetEnabled)
    {
        applyRetargetFromCurrentPose();
    }
}

void AnimationComponent::onDestroy()
{
    const bool prevEnabled = m_retargetEnabled;
    m_retargetEnabled = false;
    resolveRetargetTargetInternal();
    m_retargetEnabled = prevEnabled;
}

void AnimationComponent::setRetargetTargetObjectName(const std::string& objectName)
{
    if (m_retargetTargetObjectName == objectName)
    {
        return;
    }

    m_retargetTargetObjectName = objectName;
    m_retargetMapDirty = true;
    resolveRetargetTargetInternal();
}

bool AnimationComponent::resolveRetargetTarget()
{
    resolveRetargetTargetInternal();
    return m_retargetModel != nullptr;
}

int AnimationComponent::setupUpperBodyAdditiveLayer(int animationIndex, float weight, float speed, bool loop)
{
    if (!m_model)
    {
        return -1;
    }

    if (m_selfHumanoidMapDirty)
    {
        rebuildSelfHumanoidMap();
    }

    AnimationLayer layer;
    layer.name = "UpperBody Additive";
    layer.enabled = true;
    layer.weight = std::clamp(weight, 0.0f, 1.0f);
    layer.blendMode = LayerBlendMode::Additive;
    layer.useCurrentStatePose = false;
    layer.layerAnimationIndex = animationIndex;
    layer.layerSpeed = speed;
    layer.layerLoop = loop;
    layer.additiveAffectScale = false;
    layer.additiveAffectTranslation = false;
    layer.boneMask = collectHumanoidBoneMask(true);

    if (layer.boneMask.empty())
    {
        return -1;
    }

    return static_cast<int>(m_stateMachine.addLayer(layer));
}

int AnimationComponent::setupLowerBodyAdditiveLayer(int animationIndex, float weight, float speed, bool loop)
{
    if (!m_model)
    {
        return -1;
    }

    if (m_selfHumanoidMapDirty)
    {
        rebuildSelfHumanoidMap();
    }

    AnimationLayer layer;
    layer.name = "LowerBody Additive";
    layer.enabled = true;
    layer.weight = std::clamp(weight, 0.0f, 1.0f);
    layer.blendMode = LayerBlendMode::Additive;
    layer.useCurrentStatePose = false;
    layer.layerAnimationIndex = animationIndex;
    layer.layerSpeed = speed;
    layer.layerLoop = loop;
    layer.additiveAffectScale = false;
    layer.additiveAffectTranslation = false;
    layer.boneMask = collectHumanoidBoneMask(false);

    if (layer.boneMask.empty())
    {
        return -1;
    }

    return static_cast<int>(m_stateMachine.addLayer(layer));
}

void AnimationComponent::setFootIKEnabled(bool left, bool enabled)
{
    IKGoal& goal = left ? m_leftFootIK : m_rightFootIK;
    goal.enabled = enabled;
    goal.hasTarget = false;
    goal.hasSmoothedTarget = false;
}

void AnimationComponent::setFootIKTarget(bool left, const Vector3& worldTarget, float weight)
{
    IKGoal& goal = left ? m_leftFootIK : m_rightFootIK;
    goal.targetWorld = worldTarget;
    goal.weight = std::clamp(weight, 0.0f, 1.0f);
    goal.hasTarget = true;
    if (!goal.hasSmoothedTarget)
    {
        goal.smoothedTargetWorld = worldTarget;
        goal.hasSmoothedTarget = true;
    }
    goal.enabled = true;
}

void AnimationComponent::setArmIKEnabled(bool left, bool enabled)
{
    IKGoal& goal = left ? m_leftArmIK : m_rightArmIK;
    goal.enabled = enabled;
    goal.hasTarget = false;
    goal.hasSmoothedTarget = false;
}

void AnimationComponent::setArmIKTarget(bool left, const Vector3& worldTarget, float weight)
{
    IKGoal& goal = left ? m_leftArmIK : m_rightArmIK;
    goal.targetWorld = worldTarget;
    goal.weight = std::clamp(weight, 0.0f, 1.0f);
    goal.hasTarget = true;
    if (!goal.hasSmoothedTarget)
    {
        goal.smoothedTargetWorld = worldTarget;
        goal.hasSmoothedTarget = true;
    }
    goal.enabled = true;
}

Matrix AnimationComponent::getModelWorldMatrix() const
{
    if (const auto* tf = gameObject() ? gameObject()->getComponent<TransformComponent>() : nullptr)
    {
        return tf->getWorldMatrix();
    }
    return Matrix::Identity;
}

void AnimationComponent::rebuildSelfHumanoidMap()
{
    m_selfHumanToBone.fill(-1);

    if (!m_model)
    {
        m_selfHumanoidMapDirty = false;
        return;
    }

    const auto& bones = m_model->getResource()->getModelData().bones;
    std::array<int, HumanoidRig::BoneCount> bestScore{};
    bestScore.fill(-1000000);

    auto scoreBoneName = [](std::string_view boneName) -> int
        {
            int score = 0;
            const std::string normalized = HumanoidRig::normalizeBoneName(boneName);
            if (!normalized.empty())
            {
                score += 100;
                if (normalized.find("twist") != std::string::npos) score -= 120;
                if (normalized.find("roll") != std::string::npos) score -= 120;
                if (normalized.find("ik") != std::string::npos) score -= 120;
                if (normalized.find("helper") != std::string::npos) score -= 120;
                if (normalized.find("end") != std::string::npos) score -= 80;
                if (normalized.find("nub") != std::string::npos) score -= 80;
            }
            return score;
        };

    for (size_t i = 0; i < bones.size(); ++i)
    {
        if (HumanoidRig::isLikelyHelperBone(bones[i].name)) continue;

        const HumanBodyBone human = HumanoidRig::classify(bones[i].name);
        if (human == HumanBodyBone::Invalid) continue;

        const size_t slot = static_cast<size_t>(human);
        if (slot >= HumanoidRig::BoneCount) continue;

        const int score = scoreBoneName(bones[i].name);
        if (score > bestScore[slot])
        {
            bestScore[slot] = score;
            m_selfHumanToBone[slot] = static_cast<int>(i);
        }
    }

    const auto& runtimeBones = m_model->getBone();
    auto parentIndexOf = [&](int index) -> int
        {
            if (index < 0 || index >= static_cast<int>(runtimeBones.size())) return -1;
            const Model::Bone* parent = runtimeBones[index].parent;
            if (!parent) return -1;
            return static_cast<int>(parent - runtimeBones.data());
        };

    auto setIfMissingFromParent = [&](HumanBodyBone missing, HumanBodyBone from)
        {
            const size_t miss = static_cast<size_t>(missing);
            const size_t src = static_cast<size_t>(from);
            if (m_selfHumanToBone[miss] >= 0 || m_selfHumanToBone[src] < 0) return;
            m_selfHumanToBone[miss] = parentIndexOf(m_selfHumanToBone[src]);
        };

    setIfMissingFromParent(HumanBodyBone::Neck, HumanBodyBone::Head);
    setIfMissingFromParent(HumanBodyBone::UpperChest, HumanBodyBone::Neck);
    setIfMissingFromParent(HumanBodyBone::Chest, HumanBodyBone::UpperChest);
    setIfMissingFromParent(HumanBodyBone::Spine, HumanBodyBone::Chest);
    setIfMissingFromParent(HumanBodyBone::Hips, HumanBodyBone::Spine);

    setIfMissingFromParent(HumanBodyBone::LeftUpperArm, HumanBodyBone::LeftLowerArm);
    setIfMissingFromParent(HumanBodyBone::LeftLowerArm, HumanBodyBone::LeftHand);
    setIfMissingFromParent(HumanBodyBone::LeftShoulder, HumanBodyBone::LeftUpperArm);

    setIfMissingFromParent(HumanBodyBone::RightUpperArm, HumanBodyBone::RightLowerArm);
    setIfMissingFromParent(HumanBodyBone::RightLowerArm, HumanBodyBone::RightHand);
    setIfMissingFromParent(HumanBodyBone::RightShoulder, HumanBodyBone::RightUpperArm);

    setIfMissingFromParent(HumanBodyBone::LeftUpperLeg, HumanBodyBone::LeftLowerLeg);
    setIfMissingFromParent(HumanBodyBone::LeftLowerLeg, HumanBodyBone::LeftFoot);
    setIfMissingFromParent(HumanBodyBone::LeftFoot, HumanBodyBone::LeftToes);

    setIfMissingFromParent(HumanBodyBone::RightUpperLeg, HumanBodyBone::RightLowerLeg);
    setIfMissingFromParent(HumanBodyBone::RightLowerLeg, HumanBodyBone::RightFoot);
    setIfMissingFromParent(HumanBodyBone::RightFoot, HumanBodyBone::RightToes);

    m_selfHumanoidMapDirty = false;
}

std::vector<int> AnimationComponent::collectHumanoidBoneMask(bool upperBody) const
{
    std::vector<int> mask;
    if (!m_model) return mask;

    std::vector<HumanBodyBone> slots;
    if (upperBody)
    {
        slots = {
            HumanBodyBone::Spine,
            HumanBodyBone::Chest,
            HumanBodyBone::UpperChest,
            HumanBodyBone::Neck,
            HumanBodyBone::Head,
            HumanBodyBone::LeftShoulder,
            HumanBodyBone::LeftUpperArm,
            HumanBodyBone::LeftLowerArm,
            HumanBodyBone::LeftHand,
            HumanBodyBone::RightShoulder,
            HumanBodyBone::RightUpperArm,
            HumanBodyBone::RightLowerArm,
            HumanBodyBone::RightHand
        };
    }
    else
    {
        slots = {
            HumanBodyBone::Hips,
            HumanBodyBone::LeftUpperLeg,
            HumanBodyBone::LeftLowerLeg,
            HumanBodyBone::LeftFoot,
            HumanBodyBone::LeftToes,
            HumanBodyBone::RightUpperLeg,
            HumanBodyBone::RightLowerLeg,
            HumanBodyBone::RightFoot,
            HumanBodyBone::RightToes
        };
    }

    std::unordered_set<int> unique;
    for (HumanBodyBone slot : slots)
    {
        const size_t idx = static_cast<size_t>(slot);
        if (idx >= m_selfHumanToBone.size()) continue;
        int bone = m_selfHumanToBone[idx];
        if (bone >= 0) unique.insert(bone);
    }

    mask.reserve(unique.size());
    for (int index : unique)
    {
        mask.push_back(index);
    }
    std::sort(mask.begin(), mask.end());
    return mask;
}

void AnimationComponent::applyWorldRotationToBone(int boneIndex, const Quaternion& worldRotation)
{
    auto& bones = m_model->getMutableBone();
    if (boneIndex < 0 || boneIndex >= static_cast<int>(bones.size()))
    {
        return;
    }

    const Model::Bone& bone = bones[boneIndex];
    XMVECTOR qParentWorld = XMQuaternionIdentity();
    if (bone.parent)
    {
        Quaternion parentWorld = matrixRotation(bone.parent->worldTransform);
        qParentWorld = XMVectorSet(parentWorld.x, parentWorld.y, parentWorld.z, parentWorld.w);
    }

    XMVECTOR qWorld = XMVectorSet(worldRotation.x, worldRotation.y, worldRotation.z, worldRotation.w);
    XMVECTOR qLocal = XMQuaternionMultiply(XMQuaternionInverse(qParentWorld), qWorld);
    qLocal = XMQuaternionNormalize(qLocal);
    XMStoreFloat4(&bones[boneIndex].rotate, qLocal);
}

void AnimationComponent::solveTwoBoneIKCCD(int upperIndex, int lowerIndex, int endIndex,
    const Vector3& targetWorld,
    float weight,
    int iterationCount,
    float maxStepDegrees)
{
    if (!m_model || weight <= 0.0f) return;

    auto& bones = m_model->getMutableBone();
    if (upperIndex < 0 || lowerIndex < 0 || endIndex < 0) return;
    if (upperIndex >= static_cast<int>(bones.size()) ||
        lowerIndex >= static_cast<int>(bones.size()) ||
        endIndex >= static_cast<int>(bones.size()))
    {
        return;
    }

    const Matrix worldBase = getModelWorldMatrix();
    m_model->updateTransform(worldBase);

    const int safeIterations = std::max(1, iterationCount);
    const float clampedWeight = std::clamp(weight, 0.0f, 1.0f);
    const float stepWeight = 1.0f - std::pow(1.0f - clampedWeight, 1.0f / static_cast<float>(safeIterations));
    const float maxStepRadians = std::max(0.1f, DirectX::XMConvertToRadians(maxStepDegrees));

    auto solveJoint = [&](int jointIndex)
        {
            m_model->updateTransform(worldBase);

            const Vector3 jointPos = matrixTranslation(bones[jointIndex].worldTransform);
            const Vector3 endPos = matrixTranslation(bones[endIndex].worldTransform);

            Vector3 toEnd = endPos - jointPos;
            Vector3 toTarget = targetWorld - jointPos;
            if (toEnd.LengthSquared() <= kAxisEpsilon || toTarget.LengthSquared() <= kAxisEpsilon)
            {
                return;
            }

            toEnd.Normalize();
            toTarget.Normalize();

            XMVECTOR qDelta = quaternionFromTo(toEnd, toTarget);
            XMVECTOR axis = XMVectorSet(0, 1, 0, 0);
            float angle = 0.0f;
            XMQuaternionToAxisAngle(&axis, &angle, qDelta);
            if (std::isfinite(angle) && angle > maxStepRadians)
            {
                qDelta = XMQuaternionRotationAxis(axis, maxStepRadians);
            }
            Quaternion delta;
            XMStoreFloat4(&delta, qDelta);
            delta.Normalize();

            Quaternion jointWorld = matrixRotation(bones[jointIndex].worldTransform);
            XMVECTOR qJoint = XMVectorSet(jointWorld.x, jointWorld.y, jointWorld.z, jointWorld.w);
            XMVECTOR qTarget = XMQuaternionMultiply(qDelta, qJoint);
            XMVECTOR qNew = XMQuaternionSlerp(qJoint, qTarget, stepWeight);
            qNew = XMQuaternionNormalize(qNew);

            Quaternion newWorld;
            XMStoreFloat4(&newWorld, qNew);
            newWorld.Normalize();
            applyWorldRotationToBone(jointIndex, newWorld);
        };

    for (int i = 0; i < safeIterations; ++i)
    {
        solveJoint(lowerIndex);
        solveJoint(upperIndex);

        m_model->updateTransform(worldBase);
        const Vector3 endPos = matrixTranslation(bones[endIndex].worldTransform);
        if ((endPos - targetWorld).LengthSquared() <= 0.0004f)
        {
            break;
        }
    }
}

void AnimationComponent::applyIK()
{
    if (!m_model)
    {
        return;
    }

    const bool hasAnyIK =
        m_leftFootIK.enabled || m_rightFootIK.enabled ||
        m_leftArmIK.enabled || m_rightArmIK.enabled;
    if (!hasAnyIK)
    {
        return;
    }

    if (m_selfHumanoidMapDirty)
    {
        rebuildSelfHumanoidMap();
    }

    auto getMapped = [&](HumanBodyBone bone) -> int
        {
            const size_t idx = static_cast<size_t>(bone);
            if (idx >= m_selfHumanToBone.size()) return -1;
            return m_selfHumanToBone[idx];
        };

    auto solveIfEnabled = [&](IKGoal& goal,
        HumanBodyBone upper,
        HumanBodyBone lower,
        HumanBodyBone end)
        {
            if (!goal.enabled || goal.weight <= 0.0f) return;

            const int upperIndex = getMapped(upper);
            const int lowerIndex = getMapped(lower);
            const int endIndex = getMapped(end);
            if (upperIndex < 0 || lowerIndex < 0 || endIndex < 0) return;

            if (!goal.hasTarget)
            {
                const Matrix worldBase = getModelWorldMatrix();
                m_model->updateTransform(worldBase);
                const auto& bones = m_model->getBone();
                goal.targetWorld = matrixTranslation(bones[endIndex].worldTransform);
                goal.hasTarget = true;
                goal.smoothedTargetWorld = goal.targetWorld;
                goal.hasSmoothedTarget = true;
            }

            if (!goal.hasSmoothedTarget)
            {
                goal.smoothedTargetWorld = goal.targetWorld;
                goal.hasSmoothedTarget = true;
            }

            const float dt = std::max(TimeManager::Instance().getDeltaTime(), 0.0f);
            const float sharpness = std::max(0.0f, goal.followSharpness);
            const float alpha = 1.0f - std::exp(-sharpness * dt);
            goal.smoothedTargetWorld = goal.smoothedTargetWorld
                + (goal.targetWorld - goal.smoothedTargetWorld) * alpha;

            solveTwoBoneIKCCD(upperIndex, lowerIndex, endIndex,
                goal.smoothedTargetWorld,
                std::clamp(goal.weight, 0.0f, 1.0f),
                kIkDefaultIterations,
                goal.maxStepDegrees);
        };

    solveIfEnabled(m_leftFootIK,
        HumanBodyBone::LeftUpperLeg,
        HumanBodyBone::LeftLowerLeg,
        HumanBodyBone::LeftFoot);
    solveIfEnabled(m_rightFootIK,
        HumanBodyBone::RightUpperLeg,
        HumanBodyBone::RightLowerLeg,
        HumanBodyBone::RightFoot);
    solveIfEnabled(m_leftArmIK,
        HumanBodyBone::LeftUpperArm,
        HumanBodyBone::LeftLowerArm,
        HumanBodyBone::LeftHand);
    solveIfEnabled(m_rightArmIK,
        HumanBodyBone::RightUpperArm,
        HumanBodyBone::RightLowerArm,
        HumanBodyBone::RightHand);
}

void AnimationComponent::play(int animationIndex, bool loop, float speed)
{
    if (!m_model) return;

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return;

    m_fading = false;
    m_prevAnimIndex = -1;
    m_animationIndex = animationIndex;
    m_currentTime = 0.0f;
    m_speed = speed;
    m_loop = loop;
    m_playing = true;
    m_paused = false;
    m_finished = false;

    // ダイレクト再生時はステートマシンを無効化
    m_useStateMachine = false;
}

void AnimationComponent::play(const std::string& animationName, bool loop, float speed)
{
    int index = findAnimationIndex(animationName);
    if (index >= 0) play(index, loop, speed);
}

void AnimationComponent::addAnimation(const char* filename)
{
    if (!m_model) return;

    auto& resource = m_model->getResource();
    auto* fbx = dynamic_cast<FbxLoad*>(resource.get());
    if (!fbx) return;

    fbx->addAnimation(filename);
    m_animatorGraphDirty = true;
}

void AnimationComponent::crossFade(int animationIndex, float fadeDuration, bool loop, float speed)
{
    if (!m_model) return;

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return;

    // 同じアニメーションへのフェードは無視
    if (animationIndex == m_animationIndex && m_playing) return;

    // 現在再生中なら prev に退避
    if (m_playing && m_animationIndex >= 0)
    {
        if (m_fading)
        {
            // 現在のブレンド結果をボーンに確定させてから prev に引き継ぐ
            float t = std::clamp(m_fadeElapsed / m_fadeDuration, 0.0f, 1.0f);
            auto& bones = m_model->getMutableBone();

            std::vector<Model::Bone> prevBones = bones;
            evaluateAnimation(m_prevAnimIndex, m_prevTime, prevBones);

            std::vector<Model::Bone> currBones = bones;
            evaluateAnimation(m_animationIndex, m_currentTime, currBones);

            blendBones(prevBones, currBones, t, bones);
        }

        m_prevAnimIndex = m_animationIndex;
        m_prevTime = m_currentTime;
        m_fading = true;
        m_fadeDuration = std::max(fadeDuration, 0.001f);
        m_fadeElapsed = 0.0f;
    }
    else
    {
        m_fading = false;
    }

    m_animationIndex = animationIndex;
    m_currentTime = 0.0f;
    m_speed = speed;
    m_loop = loop;
    m_playing = true;
    m_paused = false;
    m_finished = false;

    m_useStateMachine = false;
}

void AnimationComponent::crossFade(const std::string& animationName, float fadeDuration, bool loop, float speed)
{
    int index = findAnimationIndex(animationName);
    if (index >= 0) crossFade(index, fadeDuration, loop, speed);
}

void AnimationComponent::stop()
{
    m_playing = false;
    m_paused = false;
    m_finished = false;
    m_fading = false;
}

bool AnimationComponent::saveControllerAsset() const
{
    if (m_controllerAssetPath.empty())
    {
        LOG_WARN("[AnimationComponent] Save controller skipped. Asset path is empty.");
        return false;
    }

    std::filesystem::path path = m_controllerAssetPath;
    std::error_code ec;
    std::filesystem::path parent = path.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, ec);
    }

    return AnimatorControllerAsset::save(path, m_stateMachine);
}

bool AnimationComponent::loadControllerAsset(const std::string& path)
{
    if (path.empty())
    {
        LOG_WARN("[AnimationComponent] Load controller skipped. Path is empty.");
        return false;
    }

    if (!AnimatorControllerAsset::load(path, m_stateMachine))
    {
        return false;
    }

    m_controllerAssetPath = path;
    m_animatorGraphDirty = true;
    return true;
}

bool AnimationComponent::reloadControllerAsset()
{
    if (m_controllerAssetPath.empty())
    {
        return false;
    }
    return loadControllerAsset(m_controllerAssetPath);
}

bool AnimationComponent::isPlaying() const
{
    if (m_useStateMachine) return m_stateMachine.isPlaying();
    return m_playing && !m_paused;
}

bool AnimationComponent::isFading() const
{
    if (m_useStateMachine) return m_stateMachine.isFading();
    return m_fading;
}

float AnimationComponent::getCurrentTime() const
{
    if (m_useStateMachine)
    {
        return m_stateMachine.getNormalizedTime() * m_stateMachine.getCurrentStateLength();
    }
    return m_currentTime;
}

float AnimationComponent::getNormalizedTime() const
{
    if (m_useStateMachine) return m_stateMachine.getNormalizedTime();

    if (!m_model || m_animationIndex < 0) return 0.0f;
    const auto& animations = m_model->getResource()->getModelData().animations;
    if (m_animationIndex >= static_cast<int>(animations.size())) return 0.0f;

    float length = animations[m_animationIndex].secondsLength;
    return (length > 0.0f) ? m_currentTime / length : 0.0f;
}

int AnimationComponent::getCurrentAnimationIndex() const
{
    if (m_useStateMachine)
    {
        const auto* state = m_stateMachine.getCurrentState();
        return state ? state->getPreviewAnimationIndex() : -1;
    }
    return m_animationIndex;
}

const std::string& AnimationComponent::getCurrentAnimationName() const
{
    if (m_useStateMachine) return m_stateMachine.getCurrentStateName();

    if (!m_model || m_animationIndex < 0) return s_emptyString;
    const auto& animations = m_model->getResource()->getModelData().animations;
    if (m_animationIndex >= static_cast<int>(animations.size())) return s_emptyString;
    return animations[m_animationIndex].name;
}

int AnimationComponent::findAnimationIndex(const std::string& name) const
{
    if (!m_model) return -1;

    const auto& animations = m_model->getResource()->getModelData().animations;
    for (int i = 0; i < static_cast<int>(animations.size()); ++i)
    {
        if (animations[i].name == name) return i;
    }
    return -1;
}

int AnimationComponent::getAnimationCount() const
{
    if (!m_model)
    {
        return 0;
    }

    return static_cast<int>(m_model->getResource()->getModelData().animations.size());
}

std::string AnimationComponent::getAnimationName(int animationIndex) const
{
    if (!m_model)
    {
        return std::string();
    }

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size()))
    {
        return std::string();
    }

    return animations[animationIndex].name;
}

float AnimationComponent::getAnimationLength(int animationIndex) const
{
    if (!m_model)
    {
        return 0.0f;
    }

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size()))
    {
        return 0.0f;
    }

    return std::max(0.0f, animations[animationIndex].secondsLength);
}

bool AnimationComponent::sampleAnimation(int animationIndex, float timeSeconds, bool applyRetarget)
{
    if (!m_model)
    {
        return false;
    }

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size()))
    {
        return false;
    }

    const float length = std::max(0.0f, animations[animationIndex].secondsLength);
    const float clampedTime = std::clamp(timeSeconds, 0.0f, length);

    std::vector<Model::Bone> pose;
    if (!evaluateAnimationPose(animationIndex, clampedTime, pose))
    {
        return false;
    }

    if (!applyPose(pose, applyRetarget))
    {
        return false;
    }

    // 外部駆動時に内部プレイヤー状態が競合しないよう停止状態へ揃える。
    m_animationIndex = animationIndex;
    m_currentTime = clampedTime;
    m_playing = false;
    m_paused = true;
    m_finished = false;
    m_fading = false;
    m_prevAnimIndex = -1;
    m_prevTime = 0.0f;
    m_fadeDuration = 0.0f;
    m_fadeElapsed = 0.0f;

    applyIK();

    if (applyRetarget && m_retargetEnabled)
    {
        applyRetargetFromCurrentPose();
    }

    return true;
}

bool AnimationComponent::evaluateAnimationPose(int animationIndex, float timeSeconds, std::vector<Model::Bone>& outPose) const
{
    if (!m_model)
    {
        return false;
    }

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size()))
    {
        return false;
    }

    outPose = m_model->getBone();
    const float length = std::max(0.0f, animations[animationIndex].secondsLength);
    const float clampedTime = std::clamp(timeSeconds, 0.0f, length);
    evaluateAnimation(animationIndex, clampedTime, outPose);
    return true;
}

bool AnimationComponent::applyPose(const std::vector<Model::Bone>& pose, bool applyRetarget)
{
    if (!m_model)
    {
        return false;
    }

    auto& bones = m_model->getMutableBone();
    const size_t count = std::min(bones.size(), pose.size());
    for (size_t i = 0; i < count; ++i)
    {
        bones[i].scale = pose[i].scale;
        bones[i].rotate = pose[i].rotate;
        bones[i].translate = pose[i].translate;
    }

    applyIK();

    if (applyRetarget && m_retargetEnabled)
    {
        applyRetargetFromCurrentPose();
    }

    return true;
}

void AnimationComponent::evaluateAnimation(int animIndex, float time,
    std::vector<Model::Bone>& bones) const
{
    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animIndex < 0 || animIndex >= static_cast<int>(animations.size())) return;

    const auto& anim = animations[animIndex];
    const auto& keyframes = anim.keyframes;
    if (keyframes.empty()) return;

    if (keyframes.size() == 1)
    {
        const auto& keys = keyframes[0].nodeKeys;
        size_t count = std::min(bones.size(), keys.size());
        for (size_t i = 0; i < count; ++i)
        {
            bones[i].scale = keys[i].scale;
            bones[i].rotate = keys[i].rotate;
            bones[i].translate = keys[i].translate;
        }
        return;
    }

    float clampedTime = std::clamp(time, 0.0f, anim.secondsLength);

    // 現在時間が含まれる区間を探す
    size_t frame0 = keyframes.size() - 2;
    size_t frame1 = keyframes.size() - 1;
    float  t = 1.0f;

    for (size_t i = 0; i < keyframes.size() - 1; ++i)
    {
        if (clampedTime <= keyframes[i + 1].seconds)
        {
            frame0 = i;
            frame1 = i + 1;
            float span = keyframes[frame1].seconds - keyframes[frame0].seconds;
            t = (span > 0.0f) ? (clampedTime - keyframes[frame0].seconds) / span : 0.0f;
            break;
        }
    }

    const auto& keys0 = keyframes[frame0].nodeKeys;
    const auto& keys1 = keyframes[frame1].nodeKeys;
    size_t count = std::min({ bones.size(), keys0.size(), keys1.size() });

    for (size_t i = 0; i < count; ++i)
    {
        XMVECTOR s0 = XMLoadFloat3(&keys0[i].scale);
        XMVECTOR s1 = XMLoadFloat3(&keys1[i].scale);
        XMVECTOR r0 = XMLoadFloat4(&keys0[i].rotate);
        XMVECTOR r1 = XMLoadFloat4(&keys1[i].rotate);
        XMVECTOR t0 = XMLoadFloat3(&keys0[i].translate);
        XMVECTOR t1 = XMLoadFloat3(&keys1[i].translate);

        XMStoreFloat3(&bones[i].scale, XMVectorLerp(s0, s1, t));
        XMStoreFloat4(&bones[i].rotate, XMQuaternionSlerp(r0, r1, t));
        XMStoreFloat3(&bones[i].translate, XMVectorLerp(t0, t1, t));
    }
}

void AnimationComponent::blendBones(const std::vector<Model::Bone>& a,
    const std::vector<Model::Bone>& b,
    float t,
    std::vector<Model::Bone>& out)
{
    size_t count = std::min({ a.size(), b.size(), out.size() });
    for (size_t i = 0; i < count; ++i)
    {
        XMVECTOR sA = XMLoadFloat3(&a[i].scale);
        XMVECTOR sB = XMLoadFloat3(&b[i].scale);
        XMVECTOR rA = XMLoadFloat4(&a[i].rotate);
        XMVECTOR rB = XMLoadFloat4(&b[i].rotate);
        XMVECTOR tA = XMLoadFloat3(&a[i].translate);
        XMVECTOR tB = XMLoadFloat3(&b[i].translate);

        XMStoreFloat3(&out[i].scale, XMVectorLerp(sA, sB, t));
        XMStoreFloat4(&out[i].rotate, XMQuaternionSlerp(rA, rB, t));
        XMStoreFloat3(&out[i].translate, XMVectorLerp(tA, tB, t));
    }
}

float AnimationComponent::getSamplingTime(int animIndex) const
{
    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animIndex < 0 || animIndex >= static_cast<int>(animations.size()))
        return 1.0f / 60.0f;

    const auto& kf = animations[animIndex].keyframes;
    if (kf.size() > 1)
        return kf[1].seconds - kf[0].seconds;

    return 1.0f / 60.0f;
}

void AnimationComponent::resolveRetargetTargetInternal()
{
    GameObject* previousObject = m_retargetTargetObject;
    Model* previous = m_retargetModel;
    m_retargetTargetObject = nullptr;
    m_retargetModel = nullptr;

    if (!m_retargetEnabled || m_retargetTargetObjectName.empty())
    {
        if (previousObject)
        {
            if (auto* prevAnim = previousObject->getComponent<AnimationComponent>())
            {
                if (prevAnim != this)
                {
                    prevAnim->setExternalRetargetOverride(false);
                }
            }
        }

        if (previous != nullptr)
        {
            m_retargetMapDirty = true;
        }
        return;
    }

    const auto& objects = GameObjectRegistry::Instance().getAll();
    for (GameObject* obj : objects)
    {
        if (!obj || obj == gameObject() || obj->isDestroyed()) continue;
        if (obj->getName() != m_retargetTargetObjectName) continue;

        auto* render = obj->getComponent<FbxRenderComponent>();
        if (!render) continue;

        m_retargetTargetObject = obj;
        m_retargetModel = render->getModel();
        break;
    }

    if (previousObject != m_retargetTargetObject)
    {
        if (previousObject)
        {
            if (auto* prevAnim = previousObject->getComponent<AnimationComponent>())
            {
                if (prevAnim != this)
                {
                    prevAnim->setExternalRetargetOverride(false);
                }
            }
        }

        if (m_retargetTargetObject)
        {
            if (auto* targetAnim = m_retargetTargetObject->getComponent<AnimationComponent>())
            {
                if (targetAnim != this)
                {
                    targetAnim->setExternalRetargetOverride(true);
                }
            }
        }
    }

    if (m_retargetModel != previous)
    {
        m_retargetMapDirty = true;
    }
}

void AnimationComponent::rebuildRetargetMap()
{
    m_retargetMappedBoneCount = 0;
    m_retargetRootTranslationScale = 1.0f;
    m_retargetSourceHumanToBone.fill(-1);
    m_retargetTargetHumanToBone.fill(-1);
    m_retargetTranslationScale.fill(1.0f);
    m_retargetAxisAlignValid.fill(false);

    if (!m_model || !m_retargetModel)
    {
        m_retargetMapDirty = false;
        return;
    }

    const auto& sourceResBones = m_model->getResource()->getModelData().bones;
    const auto& targetResBones = m_retargetModel->getResource()->getModelData().bones;
    const auto& sourceBones = m_model->getBone();
    const auto& targetBones = m_retargetModel->getBone();

    std::array<int, HumanoidRig::BoneCount> sourceBestScore{};
    std::array<int, HumanoidRig::BoneCount> targetBestScore{};
    sourceBestScore.fill(-1000000);
    targetBestScore.fill(-1000000);

    auto scoreBoneName = [](std::string_view boneName) -> int
        {
            int score = 0;
            const std::string normalized = HumanoidRig::normalizeBoneName(boneName);
            if (!normalized.empty())
            {
                score += 100;
                if (normalized.find("twist") != std::string::npos) score -= 120;
                if (normalized.find("roll") != std::string::npos) score -= 120;
                if (normalized.find("ik") != std::string::npos) score -= 120;
                if (normalized.find("helper") != std::string::npos) score -= 120;
                if (normalized.find("end") != std::string::npos) score -= 80;
                if (normalized.find("nub") != std::string::npos) score -= 80;
            }
            return score;
        };

    for (size_t i = 0; i < sourceResBones.size() && i < sourceBones.size(); ++i)
    {
        if (HumanoidRig::isLikelyHelperBone(sourceResBones[i].name)) continue;

        const HumanBodyBone human = HumanoidRig::classify(sourceResBones[i].name);
        if (human == HumanBodyBone::Invalid) continue;

        const size_t slot = static_cast<size_t>(human);
        if (slot >= HumanoidRig::BoneCount) continue;

        const int score = scoreBoneName(sourceResBones[i].name);
        if (score > sourceBestScore[slot])
        {
            sourceBestScore[slot] = score;
            m_retargetSourceHumanToBone[slot] = static_cast<int>(i);
            m_retargetSourceBindPose[slot].scale = sourceResBones[i].scale;
            m_retargetSourceBindPose[slot].rotate = sourceResBones[i].rotate;
            m_retargetSourceBindPose[slot].translate = sourceResBones[i].translate;
        }
    }

    for (size_t i = 0; i < targetResBones.size() && i < targetBones.size(); ++i)
    {
        if (HumanoidRig::isLikelyHelperBone(targetResBones[i].name)) continue;

        const HumanBodyBone human = HumanoidRig::classify(targetResBones[i].name);
        if (human == HumanBodyBone::Invalid) continue;

        const size_t slot = static_cast<size_t>(human);
        if (slot >= HumanoidRig::BoneCount) continue;

        const int score = scoreBoneName(targetResBones[i].name);
        if (score > targetBestScore[slot])
        {
            targetBestScore[slot] = score;
            m_retargetTargetHumanToBone[slot] = static_cast<int>(i);
            m_retargetTargetBindPose[slot].scale = targetResBones[i].scale;
            m_retargetTargetBindPose[slot].rotate = targetResBones[i].rotate;
            m_retargetTargetBindPose[slot].translate = targetResBones[i].translate;
        }
    }

    auto fillMissingFromHierarchy = [](const std::vector<Model::Bone>& bones,
        std::array<int, HumanoidRig::BoneCount>& map)
        {
            auto parentIndexOf = [&](int index) -> int
                {
                    if (index < 0 || index >= static_cast<int>(bones.size())) return -1;
                    const Model::Bone* parent = bones[index].parent;
                    if (!parent) return -1;
                    return static_cast<int>(parent - bones.data());
                };

            auto setIfMissingFromParent = [&](HumanBodyBone missing, HumanBodyBone from)
                {
                    const size_t miss = static_cast<size_t>(missing);
                    const size_t src = static_cast<size_t>(from);
                    if (map[miss] >= 0 || map[src] < 0) return;
                    map[miss] = parentIndexOf(map[src]);
                };

            auto setIfMissingFromChild = [&](HumanBodyBone missing, HumanBodyBone from)
                {
                    const size_t miss = static_cast<size_t>(missing);
                    const size_t src = static_cast<size_t>(from);
                    if (map[miss] >= 0 || map[src] < 0) return;
                    int child = map[src];
                    int parent = parentIndexOf(child);
                    if (parent >= 0) map[miss] = parent;
                };

            setIfMissingFromParent(HumanBodyBone::Neck, HumanBodyBone::Head);
            setIfMissingFromParent(HumanBodyBone::UpperChest, HumanBodyBone::Neck);
            setIfMissingFromParent(HumanBodyBone::Chest, HumanBodyBone::UpperChest);
            setIfMissingFromParent(HumanBodyBone::Spine, HumanBodyBone::Chest);
            setIfMissingFromParent(HumanBodyBone::Hips, HumanBodyBone::Spine);

            setIfMissingFromParent(HumanBodyBone::LeftUpperArm, HumanBodyBone::LeftLowerArm);
            setIfMissingFromParent(HumanBodyBone::LeftLowerArm, HumanBodyBone::LeftHand);
            setIfMissingFromParent(HumanBodyBone::LeftShoulder, HumanBodyBone::LeftUpperArm);

            setIfMissingFromParent(HumanBodyBone::RightUpperArm, HumanBodyBone::RightLowerArm);
            setIfMissingFromParent(HumanBodyBone::RightLowerArm, HumanBodyBone::RightHand);
            setIfMissingFromParent(HumanBodyBone::RightShoulder, HumanBodyBone::RightUpperArm);

            setIfMissingFromParent(HumanBodyBone::LeftUpperLeg, HumanBodyBone::LeftLowerLeg);
            setIfMissingFromParent(HumanBodyBone::LeftLowerLeg, HumanBodyBone::LeftFoot);
            setIfMissingFromParent(HumanBodyBone::LeftFoot, HumanBodyBone::LeftToes);

            setIfMissingFromParent(HumanBodyBone::RightUpperLeg, HumanBodyBone::RightLowerLeg);
            setIfMissingFromParent(HumanBodyBone::RightLowerLeg, HumanBodyBone::RightFoot);
            setIfMissingFromParent(HumanBodyBone::RightFoot, HumanBodyBone::RightToes);

            setIfMissingFromChild(HumanBodyBone::Chest, HumanBodyBone::Spine);
            setIfMissingFromChild(HumanBodyBone::UpperChest, HumanBodyBone::Chest);
        };

    fillMissingFromHierarchy(sourceBones, m_retargetSourceHumanToBone);
    fillMissingFromHierarchy(targetBones, m_retargetTargetHumanToBone);

    for (size_t slot = 0; slot < HumanoidRig::BoneCount; ++slot)
    {
        if (m_retargetSourceHumanToBone[slot] >= 0 && m_retargetTargetHumanToBone[slot] >= 0)
        {
            const int srcIdx = m_retargetSourceHumanToBone[slot];
            const int dstIdx = m_retargetTargetHumanToBone[slot];
            if (srcIdx >= 0 && srcIdx < static_cast<int>(sourceBones.size()) &&
                dstIdx >= 0 && dstIdx < static_cast<int>(targetBones.size()))
            {
                m_retargetSourceBindPose[slot].scale = sourceResBones[srcIdx].scale;
                m_retargetSourceBindPose[slot].rotate = sourceResBones[srcIdx].rotate;
                m_retargetSourceBindPose[slot].translate = sourceResBones[srcIdx].translate;

                m_retargetTargetBindPose[slot].scale = targetResBones[dstIdx].scale;
                m_retargetTargetBindPose[slot].rotate = targetResBones[dstIdx].rotate;
                m_retargetTargetBindPose[slot].translate = targetResBones[dstIdx].translate;
            }

            ++m_retargetMappedBoneCount;
        }
    }

    float srcAvg = 0.0f;
    float dstAvg = 0.0f;
    int ratioCount = 0;
    for (size_t slot = 0; slot < HumanoidRig::BoneCount; ++slot)
    {
        if (slot == static_cast<size_t>(HumanBodyBone::Hips)) continue;

        const int srcIdx = m_retargetSourceHumanToBone[slot];
        const int dstIdx = m_retargetTargetHumanToBone[slot];
        if (srcIdx < 0 || dstIdx < 0) continue;

        const float srcLen = m_retargetSourceBindPose[slot].translate.Length();
        const float dstLen = m_retargetTargetBindPose[slot].translate.Length();
        if (srcLen <= 0.0001f || dstLen <= 0.0001f) continue;

        srcAvg += srcLen;
        dstAvg += dstLen;
        ++ratioCount;
    }

    if (ratioCount > 0)
    {
        srcAvg /= static_cast<float>(ratioCount);
        dstAvg /= static_cast<float>(ratioCount);
        if (srcAvg > 0.0001f)
        {
            m_retargetRootTranslationScale = std::clamp(dstAvg / srcAvg, 0.1f, 10.0f);
        }
    }

    for (size_t slot = 0; slot < HumanoidRig::BoneCount; ++slot)
    {
        const int srcIdx = m_retargetSourceHumanToBone[slot];
        const int dstIdx = m_retargetTargetHumanToBone[slot];
        if (srcIdx < 0 || dstIdx < 0) continue;

        const float srcLen = m_retargetSourceBindPose[slot].translate.Length();
        const float dstLen = m_retargetTargetBindPose[slot].translate.Length();
        if (srcLen > 0.0001f && dstLen > 0.0001f)
        {
            m_retargetTranslationScale[slot] = std::clamp(dstLen / srcLen, 0.1f, 10.0f);
        }
        else
        {
            m_retargetTranslationScale[slot] = m_retargetRootTranslationScale;
        }
    }

    for (size_t slot = 0; slot < HumanoidRig::BoneCount; ++slot)
    {
        const int srcIdx = m_retargetSourceHumanToBone[slot];
        const int dstIdx = m_retargetTargetHumanToBone[slot];
        if (srcIdx < 0 || dstIdx < 0) continue;

        Vector3 srcForward = Vector3::Forward;
        Vector3 srcUp = Vector3::Up;
        Vector3 dstForward = Vector3::Forward;
        Vector3 dstUp = Vector3::Up;
        if (!estimateBoneAxesFromBindPose(sourceResBones, srcIdx, srcForward, srcUp)) continue;
        if (!estimateBoneAxesFromBindPose(targetResBones, dstIdx, dstForward, dstUp)) continue;

        XMVECTOR qAxis = computeAxisAlignment(srcForward, srcUp, dstForward, dstUp);
        XMStoreFloat4(&m_retargetAxisAlign[slot], qAxis);
        m_retargetAxisAlignValid[slot] = true;
    }

    m_retargetMapDirty = false;
}

void AnimationComponent::applyRetargetFromCurrentPose()
{
    if (!m_retargetEnabled || !m_model || !m_retargetModel) return;
    if (m_retargetModel == m_model) return;

    if (m_retargetMapDirty)
    {
        rebuildRetargetMap();
    }

    if (m_retargetMappedBoneCount <= 0) return;

    const auto& sourceBones = m_model->getBone();
    const auto& targetResBones = m_retargetModel->getResource()->getModelData().bones;
    auto& targetBones = m_retargetModel->getMutableBone();

    // Always start from target bind pose so unmapped/helper bones remain coherent.
    const size_t resetCount = std::min(targetBones.size(), targetResBones.size());
    for (size_t i = 0; i < resetCount; ++i)
    {
        targetBones[i].scale = targetResBones[i].scale;
        targetBones[i].rotate = targetResBones[i].rotate;
        targetBones[i].translate = targetResBones[i].translate;
    }

    for (size_t slot = 0; slot < HumanoidRig::BoneCount; ++slot)
    {
        const int srcIdx = m_retargetSourceHumanToBone[slot];
        const int dstIdx = m_retargetTargetHumanToBone[slot];
        if (srcIdx < 0 || dstIdx < 0) continue;

        if (srcIdx >= static_cast<int>(sourceBones.size()) || dstIdx >= static_cast<int>(targetBones.size()))
        {
            continue;
        }

        const auto& src = sourceBones[srcIdx];
        auto& dst = targetBones[dstIdx];
        const auto& srcBind = m_retargetSourceBindPose[slot];
        const auto& dstBind = m_retargetTargetBindPose[slot];

        XMVECTOR qSrcBind = XMLoadFloat4(&srcBind.rotate);
        XMVECTOR qSrcCurrent = XMLoadFloat4(&src.rotate);
        XMVECTOR qDstBind = XMLoadFloat4(&dstBind.rotate);

        qSrcBind = XMQuaternionNormalize(qSrcBind);
        qSrcCurrent = XMQuaternionNormalize(qSrcCurrent);
        qDstBind = XMQuaternionNormalize(qDstBind);

        // Delta in source bind space and re-apply on target bind.
        XMVECTOR qDelta = XMQuaternionMultiply(XMQuaternionInverse(qSrcBind), qSrcCurrent);
        qDelta = XMQuaternionNormalize(qDelta);

        if (slot < m_retargetAxisAlignValid.size() && m_retargetAxisAlignValid[slot])
        {
            XMVECTOR qAxis = XMLoadFloat4(&m_retargetAxisAlign[slot]);
            qAxis = XMQuaternionNormalize(qAxis);
            qDelta = XMQuaternionMultiply(qAxis, XMQuaternionMultiply(qDelta, XMQuaternionInverse(qAxis)));
            qDelta = XMQuaternionNormalize(qDelta);
        }

        XMVECTOR qDstCurrent = XMQuaternionMultiply(qDstBind, qDelta);
        qDstCurrent = XMQuaternionNormalize(qDstCurrent);

        // Keep target proportions and transfer bind-space rotation delta.
        dst.scale = dstBind.scale;
        XMStoreFloat4(&dst.rotate, qDstCurrent);

        // Propagate translation delta for all mapped humanoid bones to avoid separated neck/body.
        Vector3 delta = src.translate - srcBind.translate;
        float tScale = m_retargetTranslationScale[slot];
        if (slot == static_cast<size_t>(HumanBodyBone::Hips))
        {
            tScale = m_retargetRootTranslationScale;
        }
        dst.translate = dstBind.translate + delta * tScale;
    }

    if (m_retargetTargetObject)
    {
        if (auto* tf = m_retargetTargetObject->getComponent<TransformComponent>())
        {
            m_retargetModel->updateTransform(tf->getWorldMatrix());
        }
        else
        {
            m_retargetModel->updateTransform(Matrix::Identity);
        }
    }
}

void AnimationComponent::drawSequencer()
{
    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animations.empty()) return;

    int currentAnimIdx = getCurrentAnimationIndex();

    int32_t endFrame = 1;
    if (currentAnimIdx >= 0 && currentAnimIdx < static_cast<int>(animations.size()))
    {
        endFrame = std::max(1, static_cast<int32_t>(animations[currentAnimIdx].keyframes.size()) - 1);
    }

    float samplingTime = getSamplingTime(currentAnimIdx);
    m_seqCurrentFrame = static_cast<int32_t>(getCurrentTime() / samplingTime);

    int32_t startFrame = 0;

    if (ImGui::BeginNeoSequencer("##AnimSeq", &m_seqCurrentFrame, &startFrame, &endFrame,
        ImVec2(0, 200),
        ImGuiNeoSequencerFlags_AlwaysShowHeader))
    {
        for (int animIdx = 0; animIdx < static_cast<int>(animations.size()); ++animIdx)
        {
            const auto& anim = animations[animIdx];

            if (ImGui::BeginNeoTimelineEx(anim.name.c_str()))
            {
                int32_t firstFrame = 0;
                int32_t lastFrame = std::max(0, static_cast<int32_t>(anim.keyframes.size()) - 1);
                ImGui::NeoKeyframe(&firstFrame);
                ImGui::NeoKeyframe(&lastFrame);

                if (ImGui::IsNeoTimelineSelected(ImGuiNeoTimelineIsSelectedFlags_NewlySelected))
                {
                    if (m_useStateMachine)
                    {
                        // ステートマシンモードの場合、対応するステートに強制遷移
                        m_stateMachine.forceTransition(anim.name, 0.2f);
                    }
                    else
                    {
                        crossFade(animIdx, 0.2f, m_loop, m_speed);
                    }
                }

                ImGui::EndNeoTimeLine();
            }
        }

        ImGui::EndNeoSequencer();
    }

    // ヘッドドラッグ → 再生時間に反映（ダイレクト再生モードのみ）
    if (!m_useStateMachine && m_animationIndex >= 0 &&
        m_animationIndex < static_cast<int>(animations.size()))
    {
        int32_t expectedFrame = static_cast<int32_t>(m_currentTime / samplingTime);
        if (m_seqCurrentFrame != expectedFrame)
        {
            float maxTime = animations[m_animationIndex].secondsLength;
            m_currentTime = std::clamp(m_seqCurrentFrame * samplingTime, 0.0f, maxTime);

            auto& bones = m_model->getMutableBone();
            evaluateAnimation(m_animationIndex, m_currentTime, bones);

            if (m_retargetEnabled)
            {
                applyRetargetFromCurrentPose();
            }
        }
    }
}

void AnimationComponent::drawDebugInfo()
{
    if (m_useStateMachine)
    {
        const auto* state = m_stateMachine.getCurrentState();
        if (state)
        {
            const char* motion = state->hasBlendTree() ? "BlendTree" : "Clip";
            ImGui::Text("State   : %s (%s)", state->getName().c_str(), motion);
            ImGui::Text("Time    : %.2f / %.2f s", getCurrentTime(), m_stateMachine.getCurrentStateLength());
        }
        else
        {
            ImGui::TextDisabled("No active state");
        }

        if (isFading())
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "CrossFade in progress");
        }
        return;
    }

    const auto& animations = m_model->getResource()->getModelData().animations;
    int currentAnimIdx = getCurrentAnimationIndex();

    if (currentAnimIdx >= 0 && currentAnimIdx < static_cast<int>(animations.size()))
    {
        const auto& anim = animations[currentAnimIdx];

        ImGui::Text("Playing : %s", getCurrentAnimationName().c_str());
        ImGui::Text("Time    : %.2f / %.2f s", getCurrentTime(), anim.secondsLength);
    }
    else
    {
        ImGui::TextDisabled("No animation selected");
    }

    // クロスフェード状態
    if (isFading())
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "CrossFade in progress");
    }
}

void AnimationComponent::inspectGUI()
{
    if (!m_model)
    {
        ImGui::TextDisabled("No model (FbxRenderComponent not found)");
        return;
    }

    const auto& animations = m_model->getResource()->getModelData().animations;
    ImGui::Text("Animations: %d", static_cast<int>(animations.size()));

    ImGui::SameLine();

    // アニメーション追加読み込み
    if (ImGui::Button("Load (.fbx)"))
    {
        std::vector<std::wstring> paths;
        if (Dialog::openFile(paths, L"Load Animation", L"", false) == DialogResult::OK && !paths.empty())
        {
            std::string path = toRelativePath(paths[0]);
            addAnimation(path.c_str());
            LOG_INFO("[AnimationComponent] Loaded animation: %s", path.c_str());
        }
    }

    // モード切替
    ImGui::Separator();
    ImGui::Checkbox("Humanoid Retarget", &m_retargetEnabled);
    if (m_retargetEnabled)
    {
        if (ImGui::BeginCombo("Retarget Target", m_retargetTargetObjectName.empty() ? "<None>" : m_retargetTargetObjectName.c_str()))
        {
            bool noneSelected = m_retargetTargetObjectName.empty();
            if (ImGui::Selectable("<None>", noneSelected))
            {
                setRetargetTargetObjectName("");
            }

            const auto& objects = GameObjectRegistry::Instance().getAll();
            for (GameObject* obj : objects)
            {
                if (!obj || obj == gameObject() || obj->isDestroyed()) continue;

                auto* render = obj->getComponent<FbxRenderComponent>();
                if (!render || !render->getModel()) continue;

                const std::string& objectName = obj->getName();
                bool selected = (objectName == m_retargetTargetObjectName);
                if (ImGui::Selectable(objectName.c_str(), selected))
                {
                    setRetargetTargetObjectName(objectName);
                }
            }

            ImGui::EndCombo();
        }

        resolveRetargetTargetInternal();
        if (m_retargetMapDirty)
        {
            rebuildRetargetMap();
        }

        if (m_retargetModel)
        {
            ImGui::Text("Mapped Humanoid Bones: %d", m_retargetMappedBoneCount);
            ImGui::Text("Root Translation Scale: %.3f", m_retargetRootTranslationScale);
        }
        else
        {
            ImGui::TextDisabled("Retarget target model not found");
        }
    }

    ImGui::Separator();
    if (ImGui::TreeNodeEx("IK (Foot / Arm)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool leftFootPrev = m_leftFootIK.enabled;
        ImGui::Checkbox("Left Foot IK", &m_leftFootIK.enabled);
        if (leftFootPrev != m_leftFootIK.enabled)
        {
            m_leftFootIK.hasTarget = false;
            m_leftFootIK.hasSmoothedTarget = false;
        }
        ImGui::SliderFloat("Left Foot IK Weight", &m_leftFootIK.weight, 0.0f, 1.0f, "%.2f");
        if (ImGui::DragFloat3("Left Foot Target", &m_leftFootIK.targetWorld.x, 0.01f))
        {
            m_leftFootIK.hasTarget = true;
        }
        ImGui::SliderFloat("Left Foot Follow", &m_leftFootIK.followSharpness, 1.0f, 30.0f, "%.1f");
        ImGui::SliderFloat("Left Foot MaxStep", &m_leftFootIK.maxStepDegrees, 2.0f, 45.0f, "%.1f deg");

        bool rightFootPrev = m_rightFootIK.enabled;
        ImGui::Checkbox("Right Foot IK", &m_rightFootIK.enabled);
        if (rightFootPrev != m_rightFootIK.enabled)
        {
            m_rightFootIK.hasTarget = false;
            m_rightFootIK.hasSmoothedTarget = false;
        }
        ImGui::SliderFloat("Right Foot IK Weight", &m_rightFootIK.weight, 0.0f, 1.0f, "%.2f");
        if (ImGui::DragFloat3("Right Foot Target", &m_rightFootIK.targetWorld.x, 0.01f))
        {
            m_rightFootIK.hasTarget = true;
        }
        ImGui::SliderFloat("Right Foot Follow", &m_rightFootIK.followSharpness, 1.0f, 30.0f, "%.1f");
        ImGui::SliderFloat("Right Foot MaxStep", &m_rightFootIK.maxStepDegrees, 2.0f, 45.0f, "%.1f deg");

        ImGui::Separator();
        bool leftArmPrev = m_leftArmIK.enabled;
        ImGui::Checkbox("Left Arm IK", &m_leftArmIK.enabled);
        if (leftArmPrev != m_leftArmIK.enabled)
        {
            m_leftArmIK.hasTarget = false;
            m_leftArmIK.hasSmoothedTarget = false;
        }
        ImGui::SliderFloat("Left Arm IK Weight", &m_leftArmIK.weight, 0.0f, 1.0f, "%.2f");
        if (ImGui::DragFloat3("Left Hand Target", &m_leftArmIK.targetWorld.x, 0.01f))
        {
            m_leftArmIK.hasTarget = true;
        }
        ImGui::SliderFloat("Left Arm Follow", &m_leftArmIK.followSharpness, 1.0f, 30.0f, "%.1f");
        ImGui::SliderFloat("Left Arm MaxStep", &m_leftArmIK.maxStepDegrees, 2.0f, 45.0f, "%.1f deg");

        bool rightArmPrev = m_rightArmIK.enabled;
        ImGui::Checkbox("Right Arm IK", &m_rightArmIK.enabled);
        if (rightArmPrev != m_rightArmIK.enabled)
        {
            m_rightArmIK.hasTarget = false;
            m_rightArmIK.hasSmoothedTarget = false;
        }
        ImGui::SliderFloat("Right Arm IK Weight", &m_rightArmIK.weight, 0.0f, 1.0f, "%.2f");
        if (ImGui::DragFloat3("Right Hand Target", &m_rightArmIK.targetWorld.x, 0.01f))
        {
            m_rightArmIK.hasTarget = true;
        }
        ImGui::SliderFloat("Right Arm Follow", &m_rightArmIK.followSharpness, 1.0f, 30.0f, "%.1f");
        ImGui::SliderFloat("Right Arm MaxStep", &m_rightArmIK.maxStepDegrees, 2.0f, 45.0f, "%.1f deg");

        ImGui::TreePop();
    }

    if (m_useStateMachine)
    {
        ImGui::Separator();
        if (ImGui::TreeNodeEx("Additive Layer Tools", ImGuiTreeNodeFlags_DefaultOpen))
        {
            int selectedAnim = getCurrentAnimationIndex();
            if (selectedAnim < 0 && !animations.empty())
            {
                selectedAnim = 0;
            }

            if (ImGui::Button("Add UpperBody Additive Layer") && selectedAnim >= 0)
            {
                setupUpperBodyAdditiveLayer(selectedAnim, 1.0f, 1.0f, true);
            }
            if (ImGui::Button("Add LowerBody Additive Layer") && selectedAnim >= 0)
            {
                setupLowerBodyAdditiveLayer(selectedAnim, 1.0f, 1.0f, true);
            }

            auto& layers = m_stateMachine.getLayers();
            for (size_t i = 1; i < layers.size(); ++i)
            {
                AnimationLayer& layer = layers[i];
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode(std::format("Layer {}: {}", i, layer.name).c_str()))
                {
                    ImGui::Checkbox("Enabled", &layer.enabled);
                    ImGui::SliderFloat("Weight", &layer.weight, 0.0f, 1.0f, "%.2f");
                    ImGui::SliderFloat("Layer Speed", &layer.layerSpeed, 0.0f, 3.0f, "%.2f");
                    ImGui::Checkbox("Loop", &layer.layerLoop);
                    ImGui::Checkbox("Use Current State Pose", &layer.useCurrentStatePose);

                    int mode = static_cast<int>(layer.blendMode);
                    const char* modeItems[] = { "Override", "Additive" };
                    if (ImGui::Combo("Blend Mode", &mode, modeItems, IM_ARRAYSIZE(modeItems)))
                    {
                        layer.blendMode = static_cast<LayerBlendMode>(mode);
                    }

                    if (layer.blendMode == LayerBlendMode::Additive)
                    {
                        ImGui::Checkbox("Additive Scale", &layer.additiveAffectScale);
                        ImGui::Checkbox("Additive Translation", &layer.additiveAffectTranslation);
                    }

                    if (!layer.useCurrentStatePose)
                    {
                        const char* animPreview = "<None>";
                        if (layer.layerAnimationIndex >= 0 && layer.layerAnimationIndex < static_cast<int>(animations.size()))
                        {
                            animPreview = animations[layer.layerAnimationIndex].name.c_str();
                        }

                        if (ImGui::BeginCombo("Layer Motion", animPreview))
                        {
                            for (int a = 0; a < static_cast<int>(animations.size()); ++a)
                            {
                                bool selected = (a == layer.layerAnimationIndex);
                                if (ImGui::Selectable(animations[a].name.c_str(), selected))
                                {
                                    layer.layerAnimationIndex = a;
                                    layer.layerTime = 0.0f;
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    if (ImGui::Button("Remove Layer"))
                    {
                        m_stateMachine.removeLayer(i);
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    ImGui::Checkbox("StateMachine Mode", &m_useStateMachine);
    ImGui::SameLine();
    ImGui::Checkbox("Animator Window", &m_showAnimatorWindow);

    if (!m_useStateMachine)
    {
        // ダイレクト再生モードのUI
        ImGui::SliderFloat("Speed", &m_speed, 0.0f, 3.0f, "%.2f");
        ImGui::Checkbox("Loop", &m_loop);
        ImGui::SameLine();

        if (m_playing && !m_paused)
        {
            if (ImGui::Button("Pause"))  pause();
            ImGui::SameLine();
            if (ImGui::Button("Stop"))   stop();
        }
        else if (m_paused)
        {
            if (ImGui::Button("Resume")) resume();
            ImGui::SameLine();
            if (ImGui::Button("Stop"))   stop();
        }
        else if (m_animationIndex >= 0)
        {
            if (ImGui::Button("Play"))
                play(m_animationIndex, m_loop, m_speed);
        }
    }

    ImGui::Separator();

    // デバッグ情報
    if (ImGui::TreeNodeEx("Debug Info", ImGuiTreeNodeFlags_DefaultOpen))
    {
        drawDebugInfo();
        ImGui::TreePop();
    }

    // シーケンサー
    if (ImGui::TreeNodeEx("Sequencer", ImGuiTreeNodeFlags_DefaultOpen))
    {
        drawSequencer();
        ImGui::TreePop();
    }

    if (m_useStateMachine && m_showAnimatorWindow)
    {
        drawAnimatorWindow();
    }
}

void AnimationComponent::rebuildAnimatorGraph()
{
    auto& states = m_stateMachine.getStates();
    int index = 0;
    for (auto& statePtr : states)
    {
        AnimationState& state = *statePtr;
        Vector2 pos = state.getNodePosition();
        if (pos.LengthSquared() <= 0.0001f)
        {
            pos = { 80.0f + 260.0f * (index % 4), 80.0f + 170.0f * (index / 4) };
            state.setNodePosition(pos);
        }
        ++index;
    }

    m_animatorGraphDirty = false;
}

void AnimationComponent::drawAnimatorStateInspector(AnimationState* state)
{
    if (!state || !m_model) return;

    const auto& animations = m_model->getResource()->getModelData().animations;

    ImGui::SeparatorText("State Inspector");
    ImGui::Text("State: %s", state->getName().c_str());

    int currentAnim = state->getAnimationIndex();
    const char* preview = "<None>";
    if (currentAnim >= 0 && currentAnim < static_cast<int>(animations.size()))
    {
        preview = animations[currentAnim].name.c_str();
    }

    if (ImGui::BeginCombo("Motion Clip", preview))
    {
        if (ImGui::Selectable("<None>", currentAnim < 0))
        {
            state->setAnimationIndex(-1);
        }
        for (int i = 0; i < static_cast<int>(animations.size()); ++i)
        {
            bool selected = (i == currentAnim);
            if (ImGui::Selectable(animations[i].name.c_str(), selected))
            {
                state->setAnimationIndex(i);
            }
        }
        ImGui::EndCombo();
    }

    int loopMode = static_cast<int>(state->getLoopMode());
    const char* loopItems[] = { "Once", "Loop", "PingPong" };
    if (ImGui::Combo("Loop Mode", &loopMode, loopItems, IM_ARRAYSIZE(loopItems)))
    {
        state->setLoopMode(static_cast<LoopMode>(loopMode));
    }
    float speed = state->getSpeed();
    if (ImGui::SliderFloat("State Speed", &speed, 0.0f, 3.0f, "%.2f"))
    {
        state->setSpeed(speed);
    }

    bool useBlendTree = state->hasBlendTree();
    if (ImGui::Checkbox("Use Blend Tree", &useBlendTree))
    {
        if (useBlendTree)
        {
            state->createBlendTree(BlendTreeType::Blend1D);
        }
        else
        {
            state->clearBlendTree();
        }
    }

    if (useBlendTree)
    {
        BlendTreeData* tree = state->getBlendTree();
        int treeType = static_cast<int>(tree->type);
        const char* treeItems[] = { "1D", "2D Freeform", "2D Freeform Directional" };
        if (ImGui::Combo("Blend Type", &treeType, treeItems, IM_ARRAYSIZE(treeItems)))
        {
            tree->type = static_cast<BlendTreeType>(treeType);
        }

        if (ImGui::BeginCombo("Parameter X", tree->parameterX.empty() ? "<None>" : tree->parameterX.c_str()))
        {
            for (auto& [paramName, param] : m_stateMachine.getParameters())
            {
                if (param.type != AnimParamType::Float && param.type != AnimParamType::Int) continue;
                bool selected = (tree->parameterX == paramName);
                if (ImGui::Selectable(paramName.c_str(), selected))
                {
                    tree->parameterX = paramName;
                }
            }
            ImGui::EndCombo();
        }

        if (tree->type == BlendTreeType::Freeform2D || tree->type == BlendTreeType::FreeformDirectional2D)
        {
            if (ImGui::BeginCombo("Parameter Y", tree->parameterY.empty() ? "<None>" : tree->parameterY.c_str()))
            {
                for (auto& [paramName, param] : m_stateMachine.getParameters())
                {
                    if (param.type != AnimParamType::Float && param.type != AnimParamType::Int) continue;
                    bool selected = (tree->parameterY == paramName);
                    if (ImGui::Selectable(paramName.c_str(), selected))
                    {
                        tree->parameterY = paramName;
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (ImGui::Button("Add Blend Child"))
        {
            BlendTreeChild child;
            child.animationIndex = state->getAnimationIndex();
            child.threshold = static_cast<float>(tree->children.size());
            tree->children.push_back(child);
        }

        for (int i = 0; i < static_cast<int>(tree->children.size()); ++i)
        {
            auto& child = tree->children[i];
            ImGui::PushID(i);
            if (ImGui::TreeNode(std::format("Child {}", i).c_str()))
            {
                const char* childAnim = "<None>";
                if (child.animationIndex >= 0 && child.animationIndex < static_cast<int>(animations.size()))
                {
                    childAnim = animations[child.animationIndex].name.c_str();
                }
                if (ImGui::BeginCombo("Animation", childAnim))
                {
                    for (int a = 0; a < static_cast<int>(animations.size()); ++a)
                    {
                        bool selected = (a == child.animationIndex);
                        if (ImGui::Selectable(animations[a].name.c_str(), selected))
                        {
                            child.animationIndex = a;
                        }
                    }
                    ImGui::EndCombo();
                }

                if (tree->type == BlendTreeType::Blend1D)
                {
                    ImGui::DragFloat("Threshold", &child.threshold, 0.05f);
                }
                else
                {
                    ImGui::DragFloat2("Position", &child.position.x, 0.05f);
                }
                ImGui::SliderFloat("Time Scale", &child.timeScale, 0.1f, 3.0f, "%.2f");

                if (ImGui::Button("Remove Child"))
                {
                    tree->children.erase(tree->children.begin() + i);
                    ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    auto& transitions = state->getTransitions();
    ImGui::SeparatorText("Transitions");
    if (ImGui::Button("Add Transition"))
    {
        m_stateMachine.addTransitionUnique(state->getName(), state->getName(), 0.2f);
    }

    for (int i = 0; i < static_cast<int>(transitions.size()); ++i)
    {
        auto& trans = transitions[i];
        ImGui::PushID(i);
        if (ImGui::TreeNode(std::format("Transition {}", i).c_str()))
        {
            if (ImGui::BeginCombo("Destination", trans.destStateName.c_str()))
            {
                for (auto& s : m_stateMachine.getStates())
                {
                    bool selected = (trans.destStateName == s->getName());
                    if (ImGui::Selectable(s->getName().c_str(), selected))
                    {
                        trans.destStateName = s->getName();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::DragFloat("Fade Duration", &trans.fadeDuration, 0.01f, 0.0f, 5.0f, "%.2f s");
            ImGui::Checkbox("Has Exit Time", &trans.hasExitTime);
            if (trans.hasExitTime)
            {
                ImGui::SliderFloat("Exit Time", &trans.exitTime, 0.0f, 1.0f, "%.2f");
            }
            ImGui::Checkbox("Interruptible", &trans.interruptible);

            if (ImGui::Button("Add Condition"))
            {
                TransitionCondition c;
                if (!m_stateMachine.getParameters().empty())
                {
                    c.paramName = m_stateMachine.getParameters().begin()->first;
                }
                trans.conditions.push_back(c);
            }

            for (int c = 0; c < static_cast<int>(trans.conditions.size()); ++c)
            {
                auto& cond = trans.conditions[c];
                ImGui::PushID(c);
                ImGui::Separator();
                if (ImGui::BeginCombo("Param", cond.paramName.c_str()))
                {
                    for (auto& [name, _] : m_stateMachine.getParameters())
                    {
                        bool selected = (name == cond.paramName);
                        if (ImGui::Selectable(name.c_str(), selected))
                        {
                            cond.paramName = name;
                        }
                    }
                    ImGui::EndCombo();
                }
                int op = static_cast<int>(cond.op);
                const char* opItems[] = { ">", "<", "==", "!=" };
                if (ImGui::Combo("Op", &op, opItems, IM_ARRAYSIZE(opItems)))
                {
                    cond.op = static_cast<CompareOp>(op);
                }
                ImGui::DragFloat("Threshold", &cond.threshold, 0.05f);
                if (ImGui::Button("Remove Condition"))
                {
                    trans.conditions.erase(trans.conditions.begin() + c);
                    ImGui::PopID();
                    continue;
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Remove Transition"))
            {
                transitions.erase(transitions.begin() + i);
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void AnimationComponent::drawSelectedTransitionInspector()
{
    ImGui::SeparatorText("Selected Transition");

    std::vector<AnimationTransition>* list = nullptr;
    std::string fromName;
    if (m_selectedTransitionIndex >= 0)
    {
        AnimationState* from = m_stateMachine.findState(m_selectedTransitionFromStateName);
        if (from)
        {
            list = &from->getTransitions();
            fromName = from->getName();
        }
    }

    if (!list || m_selectedTransitionIndex < 0 || m_selectedTransitionIndex >= static_cast<int>(list->size()))
    {
        ImGui::TextDisabled("Click a transition line to edit");
        m_selectedTransitionIndex = -1;
        return;
    }

    AnimationTransition& trans = (*list)[m_selectedTransitionIndex];
    ImGui::Text("From: %s", fromName.c_str());

    if (ImGui::BeginCombo("Destination", trans.destStateName.c_str()))
    {
        for (auto& s : m_stateMachine.getStates())
        {
            bool selected = (trans.destStateName == s->getName());
            if (ImGui::Selectable(s->getName().c_str(), selected))
            {
                trans.destStateName = s->getName();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::DragFloat("Fade Duration", &trans.fadeDuration, 0.01f, 0.0f, 5.0f, "%.2f s");
    ImGui::Checkbox("Has Exit Time", &trans.hasExitTime);
    if (trans.hasExitTime)
    {
        ImGui::SliderFloat("Exit Time", &trans.exitTime, 0.0f, 1.0f, "%.2f");
    }
    ImGui::Checkbox("Interruptible", &trans.interruptible);

    if (ImGui::Button("Add Condition"))
    {
        TransitionCondition c;
        if (!m_stateMachine.getParameters().empty())
        {
            c.paramName = m_stateMachine.getParameters().begin()->first;
        }
        trans.conditions.push_back(c);
    }

    for (int c = 0; c < static_cast<int>(trans.conditions.size()); ++c)
    {
        auto& cond = trans.conditions[c];
        ImGui::PushID(c + 9000);
        ImGui::Separator();

        if (ImGui::BeginCombo("Param", cond.paramName.c_str()))
        {
            for (auto& [name, _] : m_stateMachine.getParameters())
            {
                bool selected = (name == cond.paramName);
                if (ImGui::Selectable(name.c_str(), selected))
                {
                    cond.paramName = name;
                }
            }
            ImGui::EndCombo();
        }

        int op = static_cast<int>(cond.op);
        const char* opItems[] = { ">", "<", "==", "!=" };
        if (ImGui::Combo("Op", &op, opItems, IM_ARRAYSIZE(opItems)))
        {
            cond.op = static_cast<CompareOp>(op);
        }

        ImGui::DragFloat("Threshold", &cond.threshold, 0.05f);
        if (ImGui::Button("Remove Condition"))
        {
            trans.conditions.erase(trans.conditions.begin() + c);
            ImGui::PopID();
            continue;
        }
        ImGui::PopID();
    }

    if (ImGui::Button("Remove Transition"))
    {
        list->erase(list->begin() + m_selectedTransitionIndex);
        m_selectedTransitionIndex = -1;
    }
}

void AnimationComponent::drawAnimatorWindow()
{
    if (!m_model) return;

    if (m_animatorGraphDirty)
    {
        rebuildAnimatorGraph();
    }

    std::string title = std::format("Animator##{}", reinterpret_cast<uintptr_t>(this));
    if (!ImGui::Begin(title.c_str(), &m_showAnimatorWindow))
    {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Animator Controller / Blend Tree Editor");
    ImGui::Separator();

    ImGui::BeginChild("AnimatorLeft", ImVec2(420, 0), true);

    ImGui::SeparatorText("Controller");
    char controllerAssetBuf[512] = {};
    std::snprintf(controllerAssetBuf, sizeof(controllerAssetBuf), "%s", m_controllerAssetPath.c_str());
    if (ImGui::InputText("Controller Asset", controllerAssetBuf, IM_ARRAYSIZE(controllerAssetBuf)))
    {
        m_controllerAssetPath = controllerAssetBuf;
    }
    if (ImGui::Button("Load Controller"))
    {
        if (!m_controllerAssetPath.empty())
        {
            loadControllerAsset(m_controllerAssetPath);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Controller"))
    {
        saveControllerAsset();
    }

    char newStateBuf[128] = {};
    std::snprintf(newStateBuf, sizeof(newStateBuf), "%s", m_newStateName.c_str());
    if (ImGui::InputText("New State", newStateBuf, IM_ARRAYSIZE(newStateBuf)))
    {
        m_newStateName = newStateBuf;
    }
    if (ImGui::Button("Create State") && !m_newStateName.empty())
    {
        if (!m_stateMachine.findState(m_newStateName))
        {
            int defaultAnim = m_model->getResource()->getModelData().animations.empty() ? -1 : 0;
            m_stateMachine.addState(m_newStateName, defaultAnim);
            if (m_stateMachine.getCurrentState() == nullptr)
            {
                m_stateMachine.setDefaultState(m_newStateName);
                m_stateMachine.forceTransition(m_newStateName, 0.0f);
            }
            m_selectedStateName = m_newStateName;
            m_animatorGraphDirty = true;
        }
    }

    ImGui::SeparatorText("Parameters");
    char newParamBuf[128] = {};
    std::snprintf(newParamBuf, sizeof(newParamBuf), "%s", m_newParamName.c_str());
    if (ImGui::InputText("Param Name", newParamBuf, IM_ARRAYSIZE(newParamBuf)))
    {
        m_newParamName = newParamBuf;
    }
    const char* paramTypes[] = { "Float", "Int", "Bool", "Trigger" };
    ImGui::Combo("Param Type", &m_newParamType, paramTypes, IM_ARRAYSIZE(paramTypes));
    if (ImGui::Button("Add Parameter") && !m_newParamName.empty())
    {
        m_stateMachine.addParameter(m_newParamName, static_cast<AnimParamType>(m_newParamType));
    }

    for (auto& [name, param] : m_stateMachine.getParameters())
    {
        ImGui::PushID(name.c_str());
        switch (param.type)
        {
        case AnimParamType::Float:
            ImGui::DragFloat(name.c_str(), &param.floatValue, 0.01f);
            break;
        case AnimParamType::Int:
            ImGui::DragInt(name.c_str(), &param.intValue);
            break;
        case AnimParamType::Bool:
            ImGui::Checkbox(name.c_str(), &param.boolValue);
            break;
        case AnimParamType::Trigger:
            if (ImGui::Button(name.c_str()))
            {
                m_stateMachine.setTrigger(name);
            }
            break;
        }
        ImGui::PopID();
    }

    AnimationState* selectedState = nullptr;
    if (!m_selectedStateName.empty())
    {
        selectedState = m_stateMachine.findState(m_selectedStateName);
    }

    if (!selectedState)
    {
        const auto* currentState = m_stateMachine.getCurrentState();
        if (currentState)
        {
            m_selectedStateName = currentState->getName();
            selectedState = m_stateMachine.findState(m_selectedStateName);
        }
    }

    auto makeUniqueStateName = [&](const std::string& base) -> std::string {
        if (!m_stateMachine.findState(base)) return base;
        for (int i = 1; i < 10000; ++i)
        {
            std::string candidate = std::format("{}_{}", base, i);
            if (!m_stateMachine.findState(candidate)) return candidate;
        }
        return std::format("{}_{}", base, reinterpret_cast<uintptr_t>(this));
    };

    auto duplicateStateByName = [&](const std::string& sourceName) -> bool {
        AnimationState* src = m_stateMachine.findState(sourceName);
        if (!src) return false;

        std::string dstName = makeUniqueStateName(sourceName + "_Copy");
        AnimationState* dst = m_stateMachine.addState(dstName, src->getAnimationIndex());
        if (!dst) return false;

        dst->setLoopMode(src->getLoopMode());
        dst->setSpeed(src->getSpeed());
        Vector2 srcPos = src->getNodePosition();
        dst->setNodePosition(Vector2(srcPos.x + 40.0f, srcPos.y + 40.0f));

        if (const BlendTreeData* bt = src->getBlendTree())
        {
            BlendTreeData& dstBt = dst->createBlendTree(bt->type);
            dstBt.parameterX = bt->parameterX;
            dstBt.parameterY = bt->parameterY;
            dstBt.children = bt->children;
        }

        dst->getTransitions() = src->getTransitions();
        dst->getEvents() = src->getEvents();

        m_selectedStateName = dstName;
        return true;
    };

    auto removeStateByName = [&](const std::string& name) -> bool {
        if (name.empty()) return false;
        bool removed = m_stateMachine.removeState(name);
        if (!removed) return false;

        if (m_selectedStateName == name)
        {
            m_selectedStateName.clear();
            if (const auto* current = m_stateMachine.getCurrentState())
            {
                m_selectedStateName = current->getName();
            }
            else if (!m_stateMachine.getStates().empty())
            {
                m_selectedStateName = m_stateMachine.getStates().front()->getName();
            }
        }

        if (m_selectedTransitionFromStateName == name)
        {
            m_selectedTransitionIndex = -1;
            m_selectedTransitionFromStateName.clear();
        }

        return true;
    };

    ImGui::SeparatorText("Selected Graph");
    if (selectedState)
    {
        Vector2 pos = selectedState->getNodePosition();
        float posArr[2] = { pos.x, pos.y };
        if (ImGui::DragFloat2("Position", posArr, 1.0f))
        {
            selectedState->setNodePosition(Vector2(posArr[0], posArr[1]));
        }

        if (ImGui::Button("Duplicate State"))
        {
            duplicateStateByName(selectedState->getName());
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete State"))
        {
            removeStateByName(selectedState->getName());
            selectedState = nullptr;
        }
    }
    else
    {
        ImGui::TextDisabled("No state selected");
    }

    drawAnimatorStateInspector(selectedState);
    drawSelectedTransitionInspector();

    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("AnimatorGraph", ImVec2(0, 0), true);
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x < 10.0f) canvasSize.x = 10.0f;
    if (canvasSize.y < 10.0f) canvasSize.y = 10.0f;

    auto addV2 = [](const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); };
    auto subV2 = [](const ImVec2& a, const ImVec2& b) { return ImVec2(a.x - b.x, a.y - b.y); };
    auto distSq = [](const ImVec2& a, const ImVec2& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return dx * dx + dy * dy;
    };
    auto distToSegmentSq = [&](const ImVec2& p, const ImVec2& a, const ImVec2& b) {
        float vx = b.x - a.x;
        float vy = b.y - a.y;
        float wx = p.x - a.x;
        float wy = p.y - a.y;
        float vv = vx * vx + vy * vy;
        float t = (vv > 0.0f) ? std::clamp((wx * vx + wy * vy) / vv, 0.0f, 1.0f) : 0.0f;
        ImVec2 proj = ImVec2(a.x + vx * t, a.y + vy * t);
        return distSq(p, proj);
    };
    auto bezierAt = [](const ImVec2& p0, const ImVec2& c0, const ImVec2& c1, const ImVec2& p1, float t) {
        float u = 1.0f - t;
        float w0 = u * u * u;
        float w1 = 3.0f * u * u * t;
        float w2 = 3.0f * u * t * t;
        float w3 = t * t * t;
        return ImVec2(
            p0.x * w0 + c0.x * w1 + c1.x * w2 + p1.x * w3,
            p0.y * w0 + c0.y * w1 + c1.y * w2 + p1.y * w3
        );
    };
    auto bezierDistanceSq = [&](const ImVec2& p0, const ImVec2& c0, const ImVec2& c1, const ImVec2& p1, const ImVec2& p) {
        const int segments = 24;
        float best = FLT_MAX;
        ImVec2 prev = p0;
        for (int i = 1; i <= segments; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(segments);
            ImVec2 cur = bezierAt(p0, c0, c1, p1, t);
            best = std::min(best, distToSegmentSq(p, prev, cur));
            prev = cur;
        }
        return best;
    };

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, addV2(canvasPos, canvasSize), IM_COL32(26, 30, 35, 255), 4.0f);
    drawList->AddRect(canvasPos, addV2(canvasPos, canvasSize), IM_COL32(70, 78, 88, 255), 4.0f);

    ImGui::InvisibleButton("AnimatorCanvas", canvasSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

    const bool panByMiddleDrag = ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
    const bool panByAltLeftDrag = ImGui::GetIO().KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    if (!m_dragCreatingTransition && ImGui::IsItemActive() && (panByMiddleDrag || panByAltLeftDrag))
    {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_animatorCanvasPan.x += delta.x;
        m_animatorCanvasPan.y += delta.y;
    }

    struct NodeVisual
    {
        AnimationState* state = nullptr;
        ImVec2 nodePos{};
        ImVec2 nodeSize{};
        ImVec2 inputSocket{};
        ImVec2 outputSocket{};
    };

    struct TransitionVisual
    {
        std::string fromStateName;
        int index = -1;
        ImVec2 p0{};
        ImVec2 c0{};
        ImVec2 c1{};
        ImVec2 p1{};
    };

    std::vector<NodeVisual> nodes;
    std::unordered_map<std::string, size_t> indexOf;
    std::vector<TransitionVisual> transitionLines;

    auto& states = m_stateMachine.getStates();
    nodes.reserve(states.size());
    for (auto& statePtr : states)
    {
        NodeVisual n;
        n.state = statePtr.get();
        n.nodeSize = ImVec2(170.0f, 60.0f);
        n.nodePos = addV2(canvasPos, ImVec2(m_animatorCanvasPan.x + n.state->getNodePosition().x,
            m_animatorCanvasPan.y + n.state->getNodePosition().y));
        n.inputSocket = addV2(n.nodePos, ImVec2(0.0f, n.nodeSize.y * 0.5f));
        n.outputSocket = addV2(n.nodePos, ImVec2(n.nodeSize.x, n.nodeSize.y * 0.5f));
        indexOf[n.state->getName()] = nodes.size();
        nodes.push_back(n);
    }

    const float socketRadius = 6.0f;
    const float socketHoverRadius = 8.0f;
    bool transitionConnectedByDrop = false;
    ImVec2 mousePos = ImGui::GetMousePos();

    for (NodeVisual& n : nodes)
    {
        for (int i = 0; i < static_cast<int>(n.state->getTransitions().size()); ++i)
        {
            const auto& trans = n.state->getTransitions()[i];
            auto it = indexOf.find(trans.destStateName);
            if (it == indexOf.end()) continue;

            const NodeVisual& dst = nodes[it->second];
            TransitionVisual tv;
            tv.fromStateName = n.state->getName();
            tv.index = i;
            tv.p0 = n.outputSocket;
            tv.c0 = addV2(tv.p0, ImVec2(60.0f, 0.0f));
            tv.c1 = subV2(dst.inputSocket, ImVec2(60.0f, 0.0f));
            tv.p1 = dst.inputSocket;
            transitionLines.push_back(tv);
        }
    }

    for (const auto& tv : transitionLines)
    {
        bool selected = (m_selectedTransitionIndex == tv.index)
            && (m_selectedTransitionFromStateName == tv.fromStateName);

        ImU32 color = selected ? IM_COL32(255, 228, 140, 255) : IM_COL32(160, 180, 255, 220);
        float thickness = selected ? 3.0f : 2.0f;
        drawList->AddBezierCubic(tv.p0, tv.c0, tv.c1, tv.p1, color, thickness);
    }

    for (NodeVisual& n : nodes)
    {
        AnimationState& state = *n.state;

        ImGui::SetCursorScreenPos(n.nodePos);
        ImGui::PushID(state.getName().c_str());
        ImGui::InvisibleButton("StateNode", n.nodeSize);

        bool selected = (m_selectedStateName == state.getName());
        bool hoverInSocket = distSq(mousePos, n.inputSocket) <= socketHoverRadius * socketHoverRadius;
        bool hoverOutSocket = distSq(mousePos, n.outputSocket) <= socketHoverRadius * socketHoverRadius;

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !hoverInSocket && !hoverOutSocket)
        {
            m_selectedStateName = state.getName();
            selected = true;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !hoverInSocket && !hoverOutSocket)
        {
            m_stateMachine.forceTransition(state.getName(), 0.15f);
        }

        if (!m_dragCreatingTransition && !ImGui::GetIO().KeyAlt && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)
            && !hoverInSocket && !hoverOutSocket)
        {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            Vector2 p = state.getNodePosition();
            p.x += delta.x;
            p.y += delta.y;
            state.setNodePosition(p);

            n.nodePos = addV2(n.nodePos, delta);
            n.inputSocket = addV2(n.inputSocket, delta);
            n.outputSocket = addV2(n.outputSocket, delta);
        }

        if (hoverOutSocket && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_dragCreatingTransition = true;
            m_dragFromStateName = state.getName();
            m_selectedStateName = state.getName();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
            && !hoverInSocket && !hoverOutSocket)
        {
            m_contextMenuStateName = state.getName();
            m_selectedStateName = state.getName();
            ImGui::OpenPopup("StateNodeContextMenu");
        }

        if (m_dragCreatingTransition && hoverInSocket && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if (!m_dragFromStateName.empty() && m_dragFromStateName != state.getName())
            {
                m_stateMachine.addTransitionUnique(m_dragFromStateName, state.getName(), 0.2f);
            }
            transitionConnectedByDrop = true;
            m_dragCreatingTransition = false;
            m_dragFromStateName.clear();
        }

        ImU32 bodyCol = selected ? IM_COL32(74, 136, 218, 220) : IM_COL32(60, 68, 78, 230);
        ImU32 borderCol = selected ? IM_COL32(150, 210, 255, 255) : IM_COL32(120, 128, 140, 255);
        drawList->AddRectFilled(n.nodePos, addV2(n.nodePos, n.nodeSize), bodyCol, 6.0f);
        drawList->AddRect(n.nodePos, addV2(n.nodePos, n.nodeSize), borderCol, 6.0f, 0, 2.0f);

        drawList->AddText(addV2(n.nodePos, ImVec2(10.0f, 8.0f)), IM_COL32(235, 240, 245, 255), state.getName().c_str());
        const char* motionText = state.hasBlendTree() ? "BlendTree" : "Clip";
        drawList->AddText(addV2(n.nodePos, ImVec2(10.0f, 34.0f)), IM_COL32(208, 214, 224, 255), motionText);

        drawList->AddCircleFilled(n.inputSocket,
            hoverInSocket ? socketHoverRadius : socketRadius,
            IM_COL32(140, 205, 255, 255), 16);
        drawList->AddCircleFilled(n.outputSocket,
            hoverOutSocket ? socketHoverRadius : socketRadius,
            IM_COL32(255, 200, 120, 255), 16);

        ImGui::PopID();
    }

    if (m_dragCreatingTransition)
    {
        auto it = indexOf.find(m_dragFromStateName);
        if (it != indexOf.end())
        {
            const NodeVisual& src = nodes[it->second];
            ImVec2 c0 = addV2(src.outputSocket, ImVec2(60.0f, 0.0f));
            ImVec2 c1 = subV2(mousePos, ImVec2(60.0f, 0.0f));
            drawList->AddBezierCubic(src.outputSocket, c0, c1, mousePos, IM_COL32(255, 220, 130, 230), 2.0f);
        }

        if (!transitionConnectedByDrop && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            m_dragCreatingTransition = false;
            m_dragFromStateName.clear();
        }
    }

    const float pickDistanceSq = 100.0f;
    auto pickTransitionAtMouse = [&](const ImVec2& p) -> const TransitionVisual* {
        float best = pickDistanceSq;
        const TransitionVisual* picked = nullptr;
        for (const auto& tv : transitionLines)
        {
            float d = bezierDistanceSq(tv.p0, tv.c0, tv.c1, tv.p1, p);
            if (d < best)
            {
                best = d;
                picked = &tv;
            }
        }
        return picked;
    };

    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_dragCreatingTransition)
    {
        const TransitionVisual* picked = pickTransitionAtMouse(mousePos);

        if (picked)
        {
            m_selectedTransitionFromStateName = picked->fromStateName;
            m_selectedTransitionIndex = picked->index;
        }
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        const TransitionVisual* picked = pickTransitionAtMouse(mousePos);
        if (picked)
        {
            m_selectedTransitionFromStateName = picked->fromStateName;
            m_selectedTransitionIndex = picked->index;
            ImGui::OpenPopup("TransitionContextMenu");
        }
        else
        {
            ImGui::OpenPopup("AnimatorCanvasMenu");
        }
    }

    if (ImGui::BeginPopup("TransitionContextMenu"))
    {
        std::vector<AnimationTransition>* transitions = nullptr;
        if (m_selectedTransitionIndex >= 0)
        {
            AnimationState* fromState = m_stateMachine.findState(m_selectedTransitionFromStateName);
            if (fromState)
            {
                transitions = &fromState->getTransitions();
            }
        }

        bool validSelection = transitions
            && m_selectedTransitionIndex >= 0
            && m_selectedTransitionIndex < static_cast<int>(transitions->size());

        if (!validSelection)
        {
            ImGui::TextDisabled("Transition not available");
        }
        else
        {
            if (ImGui::MenuItem("Duplicate"))
            {
                transitions->push_back((*transitions)[m_selectedTransitionIndex]);
                m_selectedTransitionIndex = static_cast<int>(transitions->size()) - 1;
            }
            if (ImGui::MenuItem("Delete"))
            {
                transitions->erase(transitions->begin() + m_selectedTransitionIndex);
                m_selectedTransitionIndex = -1;
            }
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("StateNodeContextMenu"))
    {
        AnimationState* menuState = m_stateMachine.findState(m_contextMenuStateName);
        if (!menuState)
        {
            ImGui::TextDisabled("State not available");
        }
        else
        {
            if (ImGui::MenuItem("Duplicate State"))
            {
                duplicateStateByName(m_contextMenuStateName);
            }
            if (ImGui::MenuItem("Delete State"))
            {
                removeStateByName(m_contextMenuStateName);
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AnimatorCanvasMenu"))
    {
        if (ImGui::MenuItem("Reset Pan"))
        {
            m_animatorCanvasPan = Vector2(0.0f, 0.0f);
        }
        if (ImGui::MenuItem("Create State"))
        {
            std::string name = std::format("State{}", m_stateMachine.getStates().size());
            if (!m_stateMachine.findState(name))
            {
                int defaultAnim = m_model->getResource()->getModelData().animations.empty() ? -1 : 0;
                auto* s = m_stateMachine.addState(name, defaultAnim);
                if (s)
                {
                    ImVec2 mouse = ImGui::GetMousePos();
                    s->setNodePosition(Vector2(
                        mouse.x - canvasPos.x - m_animatorCanvasPan.x,
                        mouse.y - canvasPos.y - m_animatorCanvasPan.y));
                    m_selectedStateName = name;
                }
            }
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();

    ImGui::End();
}