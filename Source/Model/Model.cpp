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

        // 名前は安全にコピーして所有する
        dst.name = src.name;

        // parentIndex の範囲チェックを行ってからポインタを設定する
        if (src.parentIndex >= 0 && static_cast<size_t>(src.parentIndex) < m_bones.size())
        {
            dst.parent = &m_bones.at(static_cast<size_t>(src.parentIndex));
            // 親が存在する場合のみ子リストに追加
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