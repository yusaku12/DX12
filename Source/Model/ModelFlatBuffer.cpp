#include "pch.h"
#include "ModelFlatBuffer.h"
#include "Generated\Model_generated.h"

namespace
{
    mdl::vec2 toFlatVec2(const Vector2& value)
    {
        return mdl::vec2(value.x, value.y);
    }

    mdl::vec3 toFlatVec3(const Vector3& value)
    {
        return mdl::vec3(value.x, value.y, value.z);
    }

    mdl::vec4 toFlatVec4(const Vector4& value)
    {
        return mdl::vec4(value.x, value.y, value.z, value.w);
    }

    mdl::matrix toFlatMatrix(const Matrix& value)
    {
        return mdl::matrix(
            value._11, value._12, value._13, value._14,
            value._21, value._22, value._23, value._24,
            value._31, value._32, value._33, value._34,
            value._41, value._42, value._43, value._44);
    }

    mdl::vertex toFlatVertex(const ModelResource::Vertex& value)
    {
        return mdl::vertex(
            toFlatVec3(value.position),
            toFlatVec3(value.normal),
            toFlatVec3(value.tangent),
            toFlatVec2(value.uv),
            value.boneIndices.x,
            value.boneIndices.y,
            value.boneIndices.z,
            value.boneIndices.w,
            value.boneWeights.x,
            value.boneWeights.y,
            value.boneWeights.z,
            value.boneWeights.w);
    }

    mdl::sub_mesh toFlatSubMesh(const ModelResource::SubMesh& value)
    {
        return mdl::sub_mesh(value.startIndex, value.indexCount, value.materialIndex);
    }

    mdl::node_key toFlatNodeKey(const ModelResource::NodeKeyData& value)
    {
        return mdl::node_key(toFlatVec3(value.scale), toFlatVec4(value.rotate), toFlatVec3(value.translate));
    }

    std::vector<uint8_t> readFileBytes(const std::filesystem::path& filePath)
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file)
        {
            return {};
        }

        const std::streamsize size = file.tellg();
        if (size <= 0)
        {
            return {};
        }

        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
        {
            return {};
        }

        return bytes;
    }
}

namespace ModelFlatBuffer
{
    bool load(const std::filesystem::path& filePath, ModelResource::Model& outModel)
    {
        const std::vector<uint8_t> bytes = readFileBytes(filePath);
        if (bytes.empty())
        {
            LOG_ERROR("[ModelFlatBuffer] Failed to read file: %s", filePath.string().c_str());
            return false;
        }

        flatbuffers::Verifier verifier(bytes.data(), bytes.size());
        const mdl::model* root = flatbuffers::GetRoot<mdl::model>(bytes.data());
        if (!root || !root->Verify(verifier))
        {
            LOG_ERROR("[ModelFlatBuffer] Invalid flatbuffer model: %s", filePath.string().c_str());
            return false;
        }

        outModel = {};

        if (const auto* bones = root->bones())
        {
            outModel.bones.reserve(bones->size());
            for (const mdl::bone* bone : *bones)
            {
                ModelResource::Bone dst{};
                dst.id = bone->id();
                dst.name = bone->name() ? bone->name()->str() : std::string{};
                dst.parentIndex = bone->parent();
                if (const auto* scale = bone->scale()) dst.scale = Vector3{ scale->x(), scale->y(), scale->z() };
                if (const auto* rotate = bone->rotate()) dst.rotate = Vector4{ rotate->x(), rotate->y(), rotate->z(), rotate->w() };
                if (const auto* translate = bone->translate()) dst.translate = Vector3{ translate->x(), translate->y(), translate->z() };
                outModel.bones.push_back(std::move(dst));
            }
        }

        if (const auto* meshes = root->meshes())
        {
            outModel.meshes.reserve(meshes->size());
            for (const mdl::mesh* mesh : *meshes)
            {
                ModelResource::Mesh dst{};
                dst.name = mesh->name() ? mesh->name()->str() : std::string{};
                dst.nodeIndex = mesh->node_index();
                if (const auto* boundsMin = mesh->bounds_min()) dst.boundsMin = DirectX::XMFLOAT3(boundsMin->x(), boundsMin->y(), boundsMin->z());
                if (const auto* boundsMax = mesh->bounds_max()) dst.boundsMax = DirectX::XMFLOAT3(boundsMax->x(), boundsMax->y(), boundsMax->z());

                if (const auto* vertices = mesh->vertices())
                {
                    dst.vertices.reserve(vertices->size());
                    for (const mdl::vertex* vertex : *vertices)
                    {
                        ModelResource::Vertex outVertex{};
                        outVertex.position = Vector3{ vertex->position().x(), vertex->position().y(), vertex->position().z() };
                        outVertex.normal = Vector3{ vertex->normal().x(), vertex->normal().y(), vertex->normal().z() };
                        outVertex.tangent = Vector3{ vertex->tangent().x(), vertex->tangent().y(), vertex->tangent().z() };
                        outVertex.uv = Vector2{ vertex->uv().x(), vertex->uv().y() };
                        outVertex.boneIndices = { vertex->bone_index0(), vertex->bone_index1(), vertex->bone_index2(), vertex->bone_index3() };
                        outVertex.boneWeights = { vertex->weight0(), vertex->weight1(), vertex->weight2(), vertex->weight3() };
                        dst.vertices.push_back(std::move(outVertex));
                    }
                }

                if (const auto* indices = mesh->indices())
                {
                    dst.indices.reserve(indices->size());
                    for (uint32_t index : *indices)
                    {
                        dst.indices.push_back(index);
                    }
                }

                if (const auto* subMeshes = mesh->sub_meshes())
                {
                    dst.subMeshes.reserve(subMeshes->size());
                    for (const mdl::sub_mesh* subMesh : *subMeshes)
                    {
                        ModelResource::SubMesh outSubMesh{};
                        outSubMesh.startIndex = subMesh->start_index();
                        outSubMesh.indexCount = subMesh->index_count();
                        outSubMesh.materialIndex = subMesh->material_index();
                        dst.subMeshes.push_back(std::move(outSubMesh));
                    }
                }

                if (const auto* nodeIndices = mesh->node_indices())
                {
                    dst.nodeIndices.reserve(nodeIndices->size());
                    for (int32_t nodeIndex : *nodeIndices)
                    {
                        dst.nodeIndices.push_back(nodeIndex);
                    }
                }

                if (const auto* offsetMatrices = mesh->offset_matrices())
                {
                    dst.offsetTransforms.reserve(offsetMatrices->size());
                    for (const mdl::matrix* offsetMatrix : *offsetMatrices)
                    {
                        dst.offsetTransforms.emplace_back(
                            offsetMatrix->m00(), offsetMatrix->m01(), offsetMatrix->m02(), offsetMatrix->m03(),
                            offsetMatrix->m10(), offsetMatrix->m11(), offsetMatrix->m12(), offsetMatrix->m13(),
                            offsetMatrix->m20(), offsetMatrix->m21(), offsetMatrix->m22(), offsetMatrix->m23(),
                            offsetMatrix->m30(), offsetMatrix->m31(), offsetMatrix->m32(), offsetMatrix->m33());
                    }
                }

                outModel.meshes.push_back(std::move(dst));
            }
        }

        if (const auto* materials = root->materials())
        {
            outModel.materials.reserve(materials->size());
            for (const mdl::material* material : *materials)
            {
                ModelResource::Material dst{};
                dst.name = material->name() ? material->name()->str() : std::string{};
                if (material->texture_albedo()) dst.textureName[0] = material->texture_albedo()->str();
                if (material->texture_normal()) dst.textureName[1] = material->texture_normal()->str();
                if (const auto* diffuse = material->diffuse_color()) dst.diffuseColor = Vector4{ diffuse->x(), diffuse->y(), diffuse->z(), diffuse->w() };
                outModel.materials.push_back(std::move(dst));
            }
        }

        if (const auto* animations = root->animations())
        {
            outModel.animations.reserve(animations->size());
            for (const mdl::animation* animation : *animations)
            {
                ModelResource::Animation dst{};
                dst.name = animation->name() ? animation->name()->str() : std::string{};
                dst.secondsLength = animation->duration();

                if (const auto* keyframes = animation->keyframes())
                {
                    dst.keyframes.reserve(keyframes->size());
                    for (const mdl::keyframe* keyframe : *keyframes)
                    {
                        ModelResource::Keyframe outKeyframe{};
                        outKeyframe.seconds = keyframe->time();

                        if (const auto* nodeKeys = keyframe->node_keys())
                        {
                            outKeyframe.nodeKeys.reserve(nodeKeys->size());
                            for (const mdl::node_key* nodeKey : *nodeKeys)
                            {
                                ModelResource::NodeKeyData outNodeKey{};
                                outNodeKey.scale = Vector3{ nodeKey->scale().x(), nodeKey->scale().y(), nodeKey->scale().z() };
                                outNodeKey.rotate = Vector4{ nodeKey->rotate().x(), nodeKey->rotate().y(), nodeKey->rotate().z(), nodeKey->rotate().w() };
                                outNodeKey.translate = Vector3{ nodeKey->translate().x(), nodeKey->translate().y(), nodeKey->translate().z() };
                                outKeyframe.nodeKeys.push_back(std::move(outNodeKey));
                            }
                        }

                        dst.keyframes.push_back(std::move(outKeyframe));
                    }
                }

                outModel.animations.push_back(std::move(dst));
            }
        }

        return true;
    }

    bool save(const std::filesystem::path& filePath, const ModelResource::Model& model)
    {
        flatbuffers::FlatBufferBuilder builder(1024 * 1024);

        std::vector<flatbuffers::Offset<mdl::bone>> bones;
        bones.reserve(model.bones.size());
        for (const auto& bone : model.bones)
        {
            mdl::vec3 scale = toFlatVec3(bone.scale);
            mdl::vec4 rotate = toFlatVec4(bone.rotate);
            mdl::vec3 translate = toFlatVec3(bone.translate);

            bones.push_back(mdl::CreateboneDirect(
                builder,
                bone.id,
                bone.name.c_str(),
                bone.parentIndex,
                &scale,
                &rotate,
                &translate));
        }

        std::vector<flatbuffers::Offset<mdl::mesh>> meshes;
        meshes.reserve(model.meshes.size());
        for (const auto& mesh : model.meshes)
        {
            std::vector<mdl::vertex> vertices;
            vertices.reserve(mesh.vertices.size());
            for (const auto& vertex : mesh.vertices)
            {
                vertices.push_back(toFlatVertex(vertex));
            }

            std::vector<uint32_t> indices = mesh.indices;

            std::vector<mdl::sub_mesh> subMeshes;
            subMeshes.reserve(mesh.subMeshes.size());
            for (const auto& subMesh : mesh.subMeshes)
            {
                subMeshes.push_back(toFlatSubMesh(subMesh));
            }

            std::vector<int32_t> nodeIndices(mesh.nodeIndices.begin(), mesh.nodeIndices.end());

            std::vector<mdl::matrix> offsetMatrices;
            offsetMatrices.reserve(mesh.offsetTransforms.size());
            for (const auto& offsetMatrix : mesh.offsetTransforms)
            {
                offsetMatrices.push_back(toFlatMatrix(offsetMatrix));
            }

            const mdl::vec3 boundsMin = toFlatVec3(Vector3{ mesh.boundsMin.x, mesh.boundsMin.y, mesh.boundsMin.z });
            const mdl::vec3 boundsMax = toFlatVec3(Vector3{ mesh.boundsMax.x, mesh.boundsMax.y, mesh.boundsMax.z });

            meshes.push_back(mdl::CreatemeshDirect(
                builder,
                mesh.name.c_str(),
                &vertices,
                &indices,
                &subMeshes,
                mesh.nodeIndex,
                &nodeIndices,
                &offsetMatrices,
                &boundsMin,
                &boundsMax));
        }

        std::vector<flatbuffers::Offset<mdl::material>> materials;
        materials.reserve(model.materials.size());
        for (const auto& material : model.materials)
        {
            const mdl::vec4 diffuseColor = toFlatVec4(material.diffuseColor);
            materials.push_back(mdl::CreatematerialDirect(
                builder,
                material.name.c_str(),
                material.textureName[0].c_str(),
                material.textureName[1].c_str(),
                &diffuseColor));
        }

        std::vector<flatbuffers::Offset<mdl::animation>> animations;
        animations.reserve(model.animations.size());
        for (const auto& animation : model.animations)
        {
            std::vector<flatbuffers::Offset<mdl::keyframe>> keyframes;
            keyframes.reserve(animation.keyframes.size());
            for (const auto& keyframe : animation.keyframes)
            {
                std::vector<mdl::node_key> nodeKeys;
                nodeKeys.reserve(keyframe.nodeKeys.size());
                for (const auto& nodeKey : keyframe.nodeKeys)
                {
                    nodeKeys.push_back(toFlatNodeKey(nodeKey));
                }

                keyframes.push_back(mdl::CreatekeyframeDirect(builder, keyframe.seconds, &nodeKeys));
            }

            animations.push_back(mdl::CreateanimationDirect(builder, animation.name.c_str(), animation.secondsLength, &keyframes));
        }

        const flatbuffers::Offset<mdl::model> root = mdl::CreatemodelDirect(builder, &bones, &meshes, &materials, &animations);
        builder.Finish(root, mdl::modelIdentifier());

        if (const std::filesystem::path parentDir = filePath.parent_path(); !parentDir.empty())
        {
            std::error_code ec;
            std::filesystem::create_directories(parentDir, ec);
        }

        std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            LOG_ERROR("[ModelFlatBuffer] Failed to open file for write: %s", filePath.string().c_str());
            return false;
        }

        file.write(reinterpret_cast<const char*>(builder.GetBufferPointer()), static_cast<std::streamsize>(builder.GetSize()));
        if (!file)
        {
            LOG_ERROR("[ModelFlatBuffer] Failed to write file: %s", filePath.string().c_str());
            return false;
        }

        return true;
    }
}