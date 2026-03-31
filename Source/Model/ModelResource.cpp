#include "pch.h"
#include "ModelResource.h"

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
        gpuMesh.vb = std::make_unique<VertexBuffer<Vertex>>(srcMesh.vertices);

        // IB
        gpuMesh.ib = std::make_unique<IndexBuffer<uint32_t>>(srcMesh.indices);

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
            // fallback
            if (!m_textures.empty())
            {
                srvIndices.push_back(m_textures[0]->getSRVIndex());
            }
            else
            {
                srvIndices.push_back(0);
            }
        }
    }

    DescriptorHeapManager::Instance().copyDescriptorsRange(subMesh.descriptorBase, srvIndices);
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