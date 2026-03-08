#include "pch.h"
#include "ModelData.h"
#include "System\Binary.h"

void ModelData::computeBounds()
{
    boundsMin = Vector3(FLT_MAX);
    boundsMax = Vector3(-FLT_MAX);

    for (auto& mesh : meshes)
    {
        mesh.boundsMin = Vector3(FLT_MAX);
        mesh.boundsMax = Vector3(-FLT_MAX);

        for (const auto& v : mesh.vertices)
        {
            mesh.boundsMin = Vector3::Min(mesh.boundsMin, v.position);
            mesh.boundsMax = Vector3::Max(mesh.boundsMax, v.position);
        }

        boundsMin = Vector3::Min(boundsMin, mesh.boundsMin);
        boundsMax = Vector3::Max(boundsMax, mesh.boundsMax);
    }
}

ModelData ModelData::importFromPMX(const PmxLoad::PMXFileData& pmx)
{
    ModelData data;
    data.name = pmx.modelInfo.modelName;

    //! マテリアル変換
    for (const auto& m : pmx.materials)
    {
        ModelMaterial mat;
        mat.name = m.name;
        mat.diffuse = m.diffuse;
        mat.specular = m.specular;
        mat.specularPower = m.specularPower;
        mat.ambient = m.ambient;

        //! Diffuse テクスチャ
        if (m.textureIndex < pmx.textures.size())
        {
            std::filesystem::path texPath = pmx.textures[m.textureIndex].textureName;
            mat.diffuseTexPath = texPath.string();
        }

        data.materials.push_back(std::move(mat));
    }

    //! 単一メッシュとして変換
    ModelMesh mesh;
    mesh.name = data.name;

    mesh.vertices.reserve(pmx.vertices.size());
    for (const auto& v : pmx.vertices)
    {
        ModelVertex gv{};
        gv.position = v.position;
        gv.normal = v.normal;
        gv.tangent = Vector4(0, 0, 0, 1);
        gv.uv = v.uv;

        for (int i = 0; i < 4; ++i)
        {
            gv.boneIndices[i] = v.boneIndices[i];
            gv.boneWeights[i] = v.boneWeights[i];
        }

        mesh.vertices.push_back(gv);
    }

    mesh.indices.reserve(pmx.faces.size() * 3);
    for (const auto& f : pmx.faces)
    {
        mesh.indices.push_back(f.vertices[0]);
        mesh.indices.push_back(f.vertices[1]);
        mesh.indices.push_back(f.vertices[2]);
    }

    //! サブメッシュ
    uint32_t startIdx = 0;
    for (uint32_t i = 0; i < (uint32_t)pmx.materials.size(); ++i)
    {
        ModelSubMesh sub;
        sub.startIndex = startIdx;
        sub.indexCount = pmx.materials[i].numFaceVertices;
        sub.materialIndex = i;
        mesh.subMeshes.push_back(sub);
        startIdx += sub.indexCount;
    }

    data.meshes.push_back(std::move(mesh));
    data.computeBounds();

    return data;
}

ModelData ModelData::importFromFBX(const FBXLoad::Model& fbx)
{
    ModelData data;

    //! マテリアル変換
    for (const auto& m : fbx.materials)
    {
        ModelMaterial mat;
        mat.name = m.name;
        mat.diffuse = m.diffuseColor;
        mat.specular = m.specularColor;
        mat.specularPower = m.specularPower;
        mat.ambient = m.ambientColor;
        mat.emissive = m.emissiveColor;
        mat.diffuseTexPath = m.texturePath;
        mat.normalTexPath = m.normalMapPath;

        data.materials.push_back(std::move(mat));
    }

    //! メッシュ変換
    for (const auto& src : fbx.meshes)
    {
        ModelMesh mesh;
        mesh.name = src.name;

        mesh.vertices.reserve(src.vertices.size());
        for (const auto& v : src.vertices)
        {
            ModelVertex gv{};
            gv.position = v.position;
            gv.normal = v.normal;
            gv.tangent = v.tangent;
            gv.uv = v.uv;

            for (int i = 0; i < 4; ++i)
            {
                gv.boneIndices[i] = v.boneIndices[i];
                gv.boneWeights[i] = v.boneWeights[i];
            }

            mesh.vertices.push_back(gv);
        }

        mesh.indices = src.indices;

        for (const auto& sub : src.subMeshes)
        {
            ModelSubMesh sm;
            sm.startIndex = sub.startIndex;
            sm.indexCount = sub.indexCount;
            sm.materialIndex = sub.materialIndex;
            mesh.subMeshes.push_back(sm);
        }

        data.meshes.push_back(std::move(mesh));
    }

    data.computeBounds();
    return data;
}

bool ModelData::saveToMdl(const std::string& path) const
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    //! Header
    Binary::writeU32(out, MDL_MAGIC);
    Binary::writeU32(out, MDL_VERSION);
    Binary::writeString(out, name);
    Binary::writeVec3(out, boundsMin);
    Binary::writeVec3(out, boundsMax);

    //! Mesh count
    Binary::writeU32(out, (uint32_t)meshes.size());

    for (const auto& mesh : meshes)
    {
        Binary::writeString(out, mesh.name);
        Binary::writeVec3(out, mesh.boundsMin);
        Binary::writeVec3(out, mesh.boundsMax);

        //! 頂点（連続メモリ一括書き込み）
        Binary::writeU32(out, (uint32_t)mesh.vertices.size());
        if (!mesh.vertices.empty())
            Binary::writeBlob(out, mesh.vertices.data(), mesh.vertices.size() * sizeof(ModelVertex));

        //! インデックス（連続メモリ一括書き込み）
        Binary::writeU32(out, (uint32_t)mesh.indices.size());
        if (!mesh.indices.empty())
            Binary::writeBlob(out, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

        //! サブメッシュ（連続メモリ一括書き込み）
        Binary::writeU32(out, (uint32_t)mesh.subMeshes.size());
        if (!mesh.subMeshes.empty())
            Binary::writeBlob(out, mesh.subMeshes.data(), mesh.subMeshes.size() * sizeof(ModelSubMesh));
    }

    //! Material count
    Binary::writeU32(out, (uint32_t)materials.size());

    for (const auto& mat : materials)
    {
        Binary::writeString(out, mat.name);
        Binary::writeVec4(out, mat.diffuse);
        Binary::writeVec3(out, mat.specular);
        Binary::writeF32(out, mat.specularPower);
        Binary::writeVec3(out, mat.ambient);
        Binary::writeVec3(out, mat.emissive);
        Binary::writeString(out, mat.diffuseTexPath);
        Binary::writeString(out, mat.normalTexPath);
        Binary::writeString(out, mat.toonTexPath);
    }

    return out.good();
}

bool ModelData::loadFromMdl(const std::string& path, ModelData& outData)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    //! Header
    uint32_t magic = Binary::readU32(in);
    if (magic != MDL_MAGIC) return false;

    uint32_t version = Binary::readU32(in);
    if (version != MDL_VERSION) return false;

    outData.name = Binary::readString(in);
    outData.boundsMin = Binary::readVec3(in);
    outData.boundsMax = Binary::readVec3(in);

    //! Meshes
    uint32_t meshCount = Binary::readU32(in);
    outData.meshes.resize(meshCount);

    for (uint32_t mi = 0; mi < meshCount; ++mi)
    {
        auto& mesh = outData.meshes[mi];
        mesh.name = Binary::readString(in);
        mesh.boundsMin = Binary::readVec3(in);
        mesh.boundsMax = Binary::readVec3(in);

        //! 頂点（一括読み込み）
        uint32_t vertCount = Binary::readU32(in);
        mesh.vertices.resize(vertCount);
        if (vertCount > 0)
            Binary::readBlob(in, mesh.vertices.data(), vertCount * sizeof(ModelVertex));

        //! インデックス（一括読み込み）
        uint32_t idxCount = Binary::readU32(in);
        mesh.indices.resize(idxCount);
        if (idxCount > 0)
            Binary::readBlob(in, mesh.indices.data(), idxCount * sizeof(uint32_t));

        //! サブメッシュ（一括読み込み）
        uint32_t subCount = Binary::readU32(in);
        mesh.subMeshes.resize(subCount);
        if (subCount > 0)
            Binary::readBlob(in, mesh.subMeshes.data(), subCount * sizeof(ModelSubMesh));
    }

    //! Materials
    uint32_t matCount = Binary::readU32(in);
    outData.materials.resize(matCount);

    for (uint32_t i = 0; i < matCount; ++i)
    {
        auto& mat = outData.materials[i];
        mat.name = Binary::readString(in);
        mat.diffuse = Binary::readVec4(in);
        mat.specular = Binary::readVec3(in);
        mat.specularPower = Binary::readF32(in);
        mat.ambient = Binary::readVec3(in);
        mat.emissive = Binary::readVec3(in);
        mat.diffuseTexPath = Binary::readString(in);
        mat.normalTexPath = Binary::readString(in);
        mat.toonTexPath = Binary::readString(in);
    }

    return in.good();
}