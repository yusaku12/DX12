#include "pch.h"
#include "ModelResource.h"
#include "ModelFlatBuffer.h"

ModelResource::~ModelResource()
{
    // サブメッシュごとに確保したディスクリプタ領域を解放する
    for (const auto& mesh : m_model.meshes)
    {
        for (const auto& sub : mesh.subMeshes)
        {
            if (sub.descriptorBase != UINT_MAX)
            {
                DescriptorHeapManager::Instance().free(sub.descriptorBase, TEXTURE_SLOT_COUNT);
            }
        }
    }
}

std::shared_ptr<ModelResource> ModelResource::createFromModelData(Model modelData)
{
    std::shared_ptr<ModelResource> resource = std::make_shared<ModelResource>();
    resource->m_model = std::move(modelData);
    return resource;
}

void ModelResource::createTextures()
{
    m_textures.clear();
    m_texturePaths.clear();

    auto findOrAddTexture = [&](const std::string& path) -> int
        {
            if (path.empty()) return -1;

            std::wstring wpath = toRelativeWPath(stringToWstring(path));

            for (int i = 0; i < static_cast<int>(m_texturePaths.size()); ++i)
            {
                if (m_texturePaths[i] == wpath)
                    return i;
            }

            auto tex = TextureManager::Instance().load(wpath);
            m_texturePaths.push_back(wpath);
            m_textures.push_back(tex);

            return static_cast<int>(m_textures.size()) - 1;
        };

    for (auto& mat : m_model.materials)
    {
        for (UINT i = 0; i < static_cast<UINT>(TextureType::Max); ++i)
        {
            findOrAddTexture(mat.textureName[i]);
        }
    }
}

void ModelResource::createMesh()
{
    m_gpuMeshes.clear();

    for (auto& srcMesh : m_model.meshes)
    {
        GpuMesh gpuMesh;

        // VB
        gpuMesh.vb = DXMem::makeUnique<VertexBuffer<Vertex>>(srcMesh.vertices);

        // IB
        gpuMesh.ib = DXMem::makeUnique<IndexBuffer<uint32_t>>(srcMesh.indices);

        // SubMesh処理
        for (auto& sub : srcMesh.subMeshes)
        {
            sub.textureIndices.fill(-1);

            if (sub.materialIndex < m_model.materials.size())
            {
                const auto& mat = m_model.materials[sub.materialIndex];

                auto findTexIndex = [&](const std::string& path) -> int
                    {
                        if (path.empty()) return -1;

                        std::wstring wpath = toRelativeWPath(stringToWstring(path));

                        for (int i = 0; i < static_cast<int>(m_texturePaths.size()); ++i)
                        {
                            if (m_texturePaths[i] == wpath)
                                return i;
                        }
                        return -1;
                    };

                sub.textureIndices[static_cast<int>(TextureType::Diffuse)] = findTexIndex(mat.textureName[static_cast<int>(TextureType::Diffuse)]);
                sub.textureIndices[static_cast<int>(TextureType::Normal)] = findTexIndex(mat.textureName[static_cast<int>(TextureType::Normal)]);
            }

            // Descriptor確保
            sub.descriptorBase = DescriptorHeapManager::Instance().allocateRange(TEXTURE_SLOT_COUNT);

            if (sub.descriptorBase == UINT_MAX)
            {
                LOG_WARN("Descriptor allocation failed for a submesh - skipping descriptor rebuild");
                continue;
            }

            // Descriptor再構築
            rebuildSubsetDescriptors(sub);
        }

        m_gpuMeshes.push_back(std::move(gpuMesh));
    }
}

void ModelResource::rebuildSubsetDescriptors(SubMesh& subMesh)
{
    if (subMesh.descriptorBase == UINT_MAX) return;

    // フォールバック用の白テクスチャを確保
    int whiteIdx = getOrCreateWhiteTextureIndex();

    std::vector<UINT> srvIndices;
    srvIndices.reserve(TEXTURE_SLOT_COUNT);

    for (UINT i = 0; i < TEXTURE_SLOT_COUNT; ++i)
    {
        int texIdx = subMesh.textureIndices[i];

        if (texIdx >= 0 && texIdx < static_cast<int>(m_textures.size()))
        {
            srvIndices.push_back(m_textures[texIdx]->getSRVIndex());
        }
        else
        {
            // 白テクスチャの SRV をフォールバックに使用
            srvIndices.push_back(m_textures[whiteIdx]->getSRVIndex());
        }
    }

    DescriptorHeapManager::Instance().copyDescriptorsRange(subMesh.descriptorBase, srvIndices);
}

int ModelResource::getOrCreateWhiteTextureIndex()
{
    // 既に "__white__" として登録済みか探す
    static const std::wstring WHITE_KEY = L"__white__";

    for (int i = 0; i < static_cast<int>(m_texturePaths.size()); ++i)
    {
        if (m_texturePaths[i] == WHITE_KEY)
            return i;
    }

    // TextureManager に白テクスチャをロードさせる（存在しないパスなので自動的に白1x1が生成される）
    LoadTexture* whiteTex = TextureManager::Instance().load(WHITE_KEY);

    int idx = static_cast<int>(m_textures.size());
    m_textures.push_back(whiteTex);
    m_texturePaths.push_back(WHITE_KEY);
    return idx;
}

bool ModelResource::replaceTexture(size_t materialIndex, TextureType texType, const std::wstring& newFilePath)
{
    if (materialIndex >= m_model.materials.size()) return false;
    if (texType >= TextureType::Max) return false;

    UINT texSlot = static_cast<UINT>(texType);

    // 新しいテクスチャをロード（TextureManager がキャッシュを管理）
    std::wstring wpath = toRelativeWPath(newFilePath);
    LoadTexture* newTex = TextureManager::Instance().load(wpath);
    if (!newTex || !newTex->isValid())
    {
        LOG_ERROR("[ModelResource] Failed to load texture: %s", wstringToString(newFilePath).c_str());
        return false;
    }

    // テクスチャ配列に追加（既に同じパスがあればそのインデックスを再利用）
    int newTexIndex = -1;
    for (int i = 0; i < static_cast<int>(m_texturePaths.size()); ++i)
    {
        if (m_texturePaths[i] == wpath)
        {
            newTexIndex = i;
            break;
        }
    }
    if (newTexIndex < 0)
    {
        newTexIndex = static_cast<int>(m_textures.size());
        m_textures.push_back(newTex);
        m_texturePaths.push_back(wpath);
    }

    // マテリアルのテクスチャ名を更新
    auto& mat = m_model.materials[materialIndex];
    mat.textureName[texSlot] = wstringToString(wpath);

    // 該当マテリアルを参照する全サブメッシュのディスクリプタを再構築
    for (auto& mesh : m_model.meshes)
    {
        for (auto& sub : mesh.subMeshes)
        {
            if (sub.materialIndex != materialIndex) continue;

            sub.textureIndices[texSlot] = newTexIndex;
            rebuildSubsetDescriptors(sub);
        }
    }

    LOG_INFO("[ModelResource] Replaced mat[%zu] %s texture -> %s",
        materialIndex, magic_enum::enum_name(texType).data(), wstringToString(wpath).c_str());

    return true;
}

void ModelResource::clearTexture(size_t materialIndex, TextureType texType)
{
    if (materialIndex >= m_model.materials.size()) return;
    if (texType >= TextureType::Max) return;

    UINT texSlot = static_cast<UINT>(texType);

    // マテリアルのテクスチャ名をクリア
    m_model.materials[materialIndex].textureName[texSlot].clear();

    // 白テクスチャのインデックスを取得
    int whiteIdx = getOrCreateWhiteTextureIndex();

    // 該当マテリアルを参照する全サブメッシュのスロットを白テクスチャに差し替え
    for (auto& mesh : m_model.meshes)
    {
        for (auto& sub : mesh.subMeshes)
        {
            if (sub.materialIndex != materialIndex) continue;

            sub.textureIndices[texSlot] = whiteIdx;
            rebuildSubsetDescriptors(sub);
        }
    }

    LOG_INFO("[ModelResource] Cleared mat[%zu] %s texture -> white fallback",
        materialIndex, magic_enum::enum_name(texType).data());
}

LoadTexture* ModelResource::getMaterialTexture(size_t materialIndex, TextureType texType) const
{
    if (materialIndex >= m_model.materials.size()) return nullptr;
    if (texType >= TextureType::Max) return nullptr;

    // マテリアルに対応するサブメッシュからテクスチャインデックスを探す
    UINT texSlot = static_cast<UINT>(texType);

    for (const auto& mesh : m_model.meshes)
    {
        for (const auto& sub : mesh.subMeshes)
        {
            if (sub.materialIndex != materialIndex) continue;

            int texIdx = sub.textureIndices[texSlot];
            if (texIdx >= 0 && texIdx < static_cast<int>(m_textures.size()))
            {
                return m_textures[texIdx];
            }
        }
    }

    return nullptr;
}

void ModelResource::computeStatistics()
{
    // 既存の drawCallCount は保持しつつ、その他をリセットして再計算する
    uint32_t prevDrawCalls = m_stats.drawCallCount;
    m_stats = {}; // 全フィールドをゼロ初期化
    m_stats.drawCallCount = prevDrawCalls;

    m_stats.meshCount = static_cast<uint32_t>(m_model.meshes.size());
    m_stats.materialCount = static_cast<uint32_t>(m_model.materials.size());
    for (const auto& mesh : m_model.meshes)
    {
        m_stats.totalVertices += static_cast<uint32_t>(mesh.vertices.size());
        m_stats.totalIndices += static_cast<uint32_t>(mesh.indices.size());
        m_stats.subMeshCount += static_cast<uint32_t>(mesh.subMeshes.size());
        m_stats.totalTriangles += static_cast<uint32_t>(mesh.indices.size() / 3);
        m_stats.drawCallCount += static_cast<uint32_t>(mesh.subMeshes.size());
    }
}

void ModelResource::bindGpuMesh(ID3D12GraphicsCommandList* cmd, size_t meshIndex) const
{
    if (!cmd) return;
    if (meshIndex >= m_gpuMeshes.size()) return;

    const auto& gpuMesh = m_gpuMeshes[meshIndex];
    if (gpuMesh.vb) gpuMesh.vb->bind(cmd);
    if (gpuMesh.ib) gpuMesh.ib->bind(cmd);
}

bool ModelResource::saveFlatBuffer(const std::filesystem::path& filePath) const
{
    return ModelFlatBuffer::save(filePath, m_model);
}