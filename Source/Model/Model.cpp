#include "pch.h"
#include "Model.h"

Model::Model(std::shared_ptr<ModelResource> resource)
    : m_resource(resource)
{
    // ノード
    const std::vector<ModelResource::Bone>& resBones = resource->getModelData().bones;

    m_bones.resize(resBones.size());
    for (size_t boneIndex = 0; boneIndex < m_bones.size(); ++boneIndex)
    {
        const auto& src = resBones.at(boneIndex);
        auto& dst = m_bones.at(boneIndex);

        dst.name = src.name;

        if (src.parentIndex >= 0 && static_cast<size_t>(src.parentIndex) < m_bones.size())
        {
            dst.parent = &m_bones.at(static_cast<size_t>(src.parentIndex));
            dst.parent->children.emplace_back(&dst);
        }
        else
        {
            dst.parent = nullptr;
        }

        dst.scale = src.scale;
        dst.rotate = src.rotate;
        dst.translate = src.translate;
    }

    rebuildLocalAABB();
}

void Model::updateTransform(const Matrix& transform)
{
    // 再帰で親→子の順に更新する
    std::function<void(Bone*, const Matrix&)> updateBone = [&](Bone* bone, const Matrix& parentTransform)
        {
            if (bone == nullptr) return;

            Matrix S = Matrix::CreateScale(bone->scale);
            Matrix R = Matrix::CreateFromQuaternion(bone->rotate);
            Matrix T = Matrix::CreateTranslation(bone->translate);
            Matrix localTransform = S * R * T;

            Matrix worldTransform = localTransform * parentTransform;

            bone->localTransform = localTransform;
            bone->worldTransform = worldTransform;

            for (Bone* child : bone->children)
            {
                updateBone(child, worldTransform);
            }
        };

    // ルートボーンから開始
    for (Bone& bone : m_bones)
    {
        if (bone.parent == nullptr)
        {
            updateBone(&bone, transform);
        }
    }
}

bool Model::getLocalAABB(Vector3& outCenter, Vector3& outExtents) const
{
    if (!m_hasLocalAABB)
    {
        return false;
    }

    outCenter = m_localAABBCenter;
    outExtents = m_localAABBExtents;
    return true;
}

void Model::rebuildLocalAABB()
{
    m_hasLocalAABB = false;
    if (!m_resource)
    {
        return;
    }

    Vector3 modelMin(std::numeric_limits<float>::max());
    Vector3 modelMax(std::numeric_limits<float>::lowest());

    const auto& modelData = m_resource->getModelData();
    std::vector<Matrix> nodeTransforms(modelData.bones.size(), Matrix::Identity);
    std::vector<uint8_t> nodeStates(modelData.bones.size(), 0);

    std::function<bool(size_t)> resolveNodeTransform = [&](size_t nodeIndex)
        {
            if (nodeStates[nodeIndex] == 2)
            {
                return true;
            }
            if (nodeStates[nodeIndex] == 1)
            {
                return false;
            }

            nodeStates[nodeIndex] = 1;
            const auto& bone = modelData.bones[nodeIndex];
            const Matrix localTransform = Matrix::CreateScale(bone.scale)
                * Matrix::CreateFromQuaternion(bone.rotate)
                * Matrix::CreateTranslation(bone.translate);

            const bool hasParent = bone.parentIndex >= 0
                && static_cast<size_t>(bone.parentIndex) < modelData.bones.size()
                && static_cast<size_t>(bone.parentIndex) != nodeIndex;

            if (hasParent)
            {
                const size_t parentIndex = static_cast<size_t>(bone.parentIndex);
                if (!resolveNodeTransform(parentIndex))
                {
                    nodeStates[nodeIndex] = 0;
                    return false;
                }
                nodeTransforms[nodeIndex] = localTransform * nodeTransforms[parentIndex];
            }
            else
            {
                nodeTransforms[nodeIndex] = localTransform;
            }

            nodeStates[nodeIndex] = 2;
            return true;
        };

    for (const auto& mesh : modelData.meshes)
    {
        if (mesh.vertices.empty())
        {
            continue;
        }

        const Vector3 meshMin(mesh.boundsMin.x, mesh.boundsMin.y, mesh.boundsMin.z);
        const Vector3 meshMax(mesh.boundsMax.x, mesh.boundsMax.y, mesh.boundsMax.z);
        const std::array<Vector3, 8> corners = {
            Vector3(meshMin.x, meshMin.y, meshMin.z),
            Vector3(meshMax.x, meshMin.y, meshMin.z),
            Vector3(meshMin.x, meshMax.y, meshMin.z),
            Vector3(meshMax.x, meshMax.y, meshMin.z),
            Vector3(meshMin.x, meshMin.y, meshMax.z),
            Vector3(meshMax.x, meshMin.y, meshMax.z),
            Vector3(meshMin.x, meshMax.y, meshMax.z),
            Vector3(meshMax.x, meshMax.y, meshMax.z),
        };

        const Matrix* nodeTransform = nullptr;
        if (mesh.nodeIndex >= 0 && static_cast<size_t>(mesh.nodeIndex) < nodeTransforms.size())
        {
            const size_t nodeIndex = static_cast<size_t>(mesh.nodeIndex);
            if (resolveNodeTransform(nodeIndex))
            {
                nodeTransform = &nodeTransforms[nodeIndex];
            }
        }

        for (const Vector3& corner : corners)
        {
            const Vector3 modelPoint = nodeTransform ? Vector3::Transform(corner, *nodeTransform) : corner;
            modelMin = Vector3::Min(modelMin, modelPoint);
            modelMax = Vector3::Max(modelMax, modelPoint);
        }

        m_hasLocalAABB = true;
    }

    if (m_hasLocalAABB)
    {
        m_localAABBCenter = (modelMin + modelMax) * 0.5f;
        m_localAABBExtents = (modelMax - modelMin) * 0.5f;
    }
}