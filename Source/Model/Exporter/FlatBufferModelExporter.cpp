#include "pch.h"
#include "FlatBufferModelExporter.h"
#include "Generated/Model_generated.h"

//--------------------------------------------------------------
// ヘルパー : SimpleMath → FlatBuffer 構造体変換
//--------------------------------------------------------------
namespace
{
    mdl::vec2 toVec2(const Vector2& v)
    {
        return mdl::vec2(v.x, v.y);
    }

    mdl::vec3 toVec3(const Vector3& v)
    {
        return mdl::vec3(v.x, v.y, v.z);
    }

    mdl::vec3 toVec3(const DirectX::XMFLOAT3& v)
    {
        return mdl::vec3(v.x, v.y, v.z);
    }

    mdl::vec4 toVec4(const Vector4& v)
    {
        return mdl::vec4(v.x, v.y, v.z, v.w);
    }

    mdl::matrix toMatrix(const Matrix& m)
    {
        return mdl::matrix(
            m._11, m._12, m._13, m._14,
            m._21, m._22, m._23, m._24,
            m._31, m._32, m._33, m._34,
            m._41, m._42, m._43, m._44
        );
    }
}

//--------------------------------------------------------------
// ボーンデータを構築
//--------------------------------------------------------------
static std::vector<flatbuffers::Offset<mdl::bone>> buildBones(
    flatbuffers::FlatBufferBuilder& fbb,
    const std::vector<ModelResource::Bone>& bones)
{
    std::vector<flatbuffers::Offset<mdl::bone>> result;
    result.reserve(bones.size());

    for (const auto& src : bones)
    {
        auto name = fbb.CreateString(src.name);
        mdl::vec3 scale     = toVec3(src.scale);
        mdl::vec4 rotate    = toVec4(src.rotate);
        mdl::vec3 translate = toVec3(src.translate);

        result.push_back(
            mdl::Createbone(fbb, src.id, name, src.parentIndex, &scale, &rotate, &translate));
    }

    return result;
}

//--------------------------------------------------------------
// メッシュデータを構築
//--------------------------------------------------------------
static std::vector<flatbuffers::Offset<mdl::mesh>> buildMeshes(
    flatbuffers::FlatBufferBuilder& fbb,
    const std::vector<ModelResource::Mesh>& meshes)
{
    std::vector<flatbuffers::Offset<mdl::mesh>> result;
    result.reserve(meshes.size());

    for (const auto& src : meshes)
    {
        // 頂点
        std::vector<mdl::vertex> verts;
        verts.reserve(src.vertices.size());
        for (const auto& v : src.vertices)
        {
            verts.emplace_back(
                toVec3(v.position),
                toVec3(v.normal),
                toVec3(v.tangent),
                toVec2(v.uv),
                v.boneIndices.x, v.boneIndices.y,
                v.boneIndices.z, v.boneIndices.w,
                v.boneWeights.x, v.boneWeights.y,
                v.boneWeights.z, v.boneWeights.w
            );
        }

        // サブメッシュ
        std::vector<mdl::sub_mesh> subs;
        subs.reserve(src.subMeshes.size());
        for (const auto& s : src.subMeshes)
        {
            subs.emplace_back(s.startIndex, s.indexCount, s.materialIndex);
        }

        // オフセット行列
        std::vector<mdl::matrix> offsets;
        offsets.reserve(src.offsetTransforms.size());
        for (const auto& m : src.offsetTransforms)
        {
            offsets.push_back(toMatrix(m));
        }

        // バウンディング
        mdl::vec3 bMin = toVec3(src.boundsMin);
        mdl::vec3 bMax = toVec3(src.boundsMax);

        result.push_back(
            mdl::CreatemeshDirect(
                fbb,
                src.name.c_str(),
                &verts,
                &src.indices,
                &subs,
                src.nodeIndex,
                &src.nodeIndices,
                &offsets,
                &bMin,
                &bMax));
    }

    return result;
}

//--------------------------------------------------------------
// マテリアルデータを構築
//--------------------------------------------------------------
static std::vector<flatbuffers::Offset<mdl::material>> buildMaterials(
    flatbuffers::FlatBufferBuilder& fbb,
    const std::vector<ModelResource::Material>& materials)
{
    std::vector<flatbuffers::Offset<mdl::material>> result;
    result.reserve(materials.size());

    for (const auto& src : materials)
    {
        const auto& albedo = src.textureName[static_cast<int>(TextureType::Diffuse)];
        const auto& normal = src.textureName[static_cast<int>(TextureType::Normal)];
        mdl::vec4 diffuse  = toVec4(src.diffuseColor);

        result.push_back(
            mdl::CreatematerialDirect(
                fbb,
                src.name.c_str(),
                albedo.empty() ? nullptr : albedo.c_str(),
                normal.empty() ? nullptr : normal.c_str(),
                &diffuse));
    }

    return result;
}

//--------------------------------------------------------------
// アニメーションデータを構築
//--------------------------------------------------------------
static std::vector<flatbuffers::Offset<mdl::animation>> buildAnimations(
    flatbuffers::FlatBufferBuilder& fbb,
    const std::vector<ModelResource::Animation>& animations)
{
    std::vector<flatbuffers::Offset<mdl::animation>> result;
    result.reserve(animations.size());

    for (const auto& src : animations)
    {
        // キーフレーム
        std::vector<flatbuffers::Offset<mdl::keyframe>> kfOffsets;
        kfOffsets.reserve(src.keyframes.size());

        for (const auto& kf : src.keyframes)
        {
            std::vector<mdl::node_key> keys;
            keys.reserve(kf.nodeKeys.size());
            for (const auto& nk : kf.nodeKeys)
            {
                keys.emplace_back(toVec3(nk.scale), toVec4(nk.rotate), toVec3(nk.translate));
            }

            kfOffsets.push_back(mdl::CreatekeyframeDirect(fbb, kf.seconds, &keys));
        }

        result.push_back(
            mdl::CreateanimationDirect(
                fbb,
                src.name.c_str(),
                src.secondsLength,
                &kfOffsets));
    }

    return result;
}

//==============================================================
// エクスポート実行
//==============================================================
bool FlatBufferModelExporter::exportModel(const ModelResource::Model& model, const std::string& filePath)
{
    flatbuffers::FlatBufferBuilder fbb(1024);

    // 各データを構築
    auto bones      = buildBones(fbb, model.bones);
    auto meshes     = buildMeshes(fbb, model.meshes);
    auto materials  = buildMaterials(fbb, model.materials);
    auto animations = buildAnimations(fbb, model.animations);

    // ルートテーブル作成
    auto root = mdl::CreatemodelDirect(fbb, &bones, &meshes, &materials, &animations);

    // "MDL0" 識別子付きでバッファを確定
    mdl::FinishmodelBuffer(fbb, root);

    // 検証
    flatbuffers::Verifier verifier(fbb.GetBufferPointer(), fbb.GetSize());
    if (!mdl::VerifymodelBuffer(verifier))
    {
        LOG_ERROR("[FlatBufferModelExporter] Buffer verification failed");
        return false;
    }

    // ファイル書き出し
    std::ofstream ofs(filePath, std::ios::binary | std::ios::trunc);
    if (!ofs)
    {
        LOG_ERROR("[FlatBufferModelExporter] Cannot open file: %s", filePath.c_str());
        return false;
    }

    ofs.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()),
              static_cast<std::streamsize>(fbb.GetSize()));
    ofs.close();

    LOG_INFO("[FlatBufferModelExporter] Exported %zu bytes -> %s", fbb.GetSize(), filePath.c_str());
    return true;
}
