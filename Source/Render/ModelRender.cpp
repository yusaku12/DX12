#include "pch.h"
#include "ModelRender.h"
#include "Component\TransformComponent.h"

ModelRender::ModelRender(const std::string& mdlPath)
{
    if (!ModelData::loadFromMdl(mdlPath, m_modelData))
    {
        LOG_ASSERT_NO_JUDGE("Failed to load .mdl: %s", mdlPath.c_str());
        return;
    }

    buildGPUResources();
}

void ModelRender::buildGPUResources()
{
    //! モデル行列CBV
    m_modelCB = std::make_unique<ConstantBuffer<ModelCB>>();
    m_modelCB->update(ModelCB{});

    //! テクスチャ読み込み
    createTextures();

    //! メッシュ毎に VB / IB / Subsets を構築
    for (const auto& srcMesh : m_modelData.meshes)
    {
        MeshDrawData meshDraw;

        //! 頂点バッファ（ModelVertex はそのまま GPU 送信可能）
        meshDraw.vertexBuffer = std::make_unique<VertexBuffer<ModelVertex>>(srcMesh.vertices);

        //! インデックスバッファ
        meshDraw.indexBuffer = std::make_unique<IndexBuffer<uint32_t>>(srcMesh.indices);

        //! サブセット
        for (const auto& sub : srcMesh.subMeshes)
        {
            Subset s{};
            s.startIndex = sub.startIndex;
            s.indexCount = sub.indexCount;
            s.materialIndex = sub.materialIndex;
            s.textureIndices.fill(-1);

            //! マテリアルに紐づくテクスチャを検索
            if (sub.materialIndex >= 0 && sub.materialIndex < m_modelData.materials.size())
            {
                const auto& mat = m_modelData.materials[sub.materialIndex];

                auto findTexIndex = [&](const std::string& path) -> int
                    {
                        if (path.empty()) return -1;
                        std::wstring wpath(path.begin(), path.end());
                        for (int i = 0; i < (int)m_texturePaths.size(); ++i)
                        {
                            if (m_texturePaths[i] == wpath) return i;
                        }
                        return -1;
                    };

                s.textureIndices[static_cast<int>(TextureType::Diffuse)] = findTexIndex(mat.texturePath[0]);
                s.textureIndices[static_cast<int>(TextureType::Normal)] = findTexIndex(mat.texturePath[1]);
            }

            s.descriptorBase = DescriptorHeapManager::Instance().allocateRange(static_cast<int>(TextureType::Max));
            rebuildSubsetDescriptors(s);

            meshDraw.subsets.push_back(s);
        }

        m_meshes.push_back(std::move(meshDraw));
    }

    //! マテリアルCBV
    createMaterialCBV();

    //! PSO
    createPSO();
}

void ModelRender::createMaterialCBV()
{
    UINT matCount = (UINT)m_modelData.materials.size();
    if (matCount == 0) matCount = 1;

    m_materialCB = std::make_unique<ConstantBuffer<MaterialCB>>(matCount);

    for (UINT i = 0; i < (UINT)m_modelData.materials.size(); ++i)
    {
        const auto& m = m_modelData.materials[i];
        MaterialCB cb{};
        cb.diffuse = m.diffuse;
        cb.specular = m.specular;
        cb.specularPower = m.specularPower;
        cb.ambient = m.ambient;
        cb.emissive = m.emissive;
        m_materialCB->update(cb, i);
    }
}

void ModelRender::createTextures()
{
    auto addTexture = [&](const std::string& path)
        {
            if (path.empty()) return;

            std::wstring wpath(path.begin(), path.end());

            for (const auto& existing : m_texturePaths)
            {
                if (existing == wpath) return;
            }

            auto texture = TextureManager::Instance().load(wpath);
            if (texture)
            {
                m_textures.push_back(texture);
                m_texturePaths.push_back(wpath);
            }
        };

    for (const auto& mat : m_modelData.materials)
    {
        addTexture(mat.texturePath[0]);
        addTexture(mat.texturePath[1]);
    }
}

void ModelRender::createPSO()
{
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::PMXStandard;
    psoData.vsShaderId = ShaderID::FBXVS;
    psoData.psShaderId = ShaderID::FBXPS;
    psoData.rasterizerState = RasterizerState::CULL_COUNTER_CLOCKWISE;
    psoData.blendState = BlendState::ALPHA;
    psoData.depthStencilState = DepthStencilState::DEPTH_DEFALT;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout =
    {
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDEX",  0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_psoKey = PSOCreator::Instance().registerPSO(psoData);
}

void ModelRender::rebuildSubsetDescriptors(Subset& subset)
{
    if (subset.descriptorBase == UINT_MAX) return;

    std::vector<UINT> srvIndices;
    srvIndices.reserve(TEXTURE_SLOT_COUNT);

    for (UINT i = 0; i < TEXTURE_SLOT_COUNT; ++i)
    {
        int texIdx = subset.textureIndices[i];

        if (texIdx >= 0 && texIdx < (int)m_textures.size())
        {
            srvIndices.push_back(m_textures[texIdx]->getSRVIndex());
        }
        else
        {
            srvIndices.push_back(m_textures[0]->getSRVIndex());
        }
    }

    DescriptorHeapManager::Instance().copyDescriptorsRange(subset.descriptorBase, srvIndices);
}

void ModelRender::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();
    render(cmd);
}

void ModelRender::render(ID3D12GraphicsCommandList* cmd)
{
    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::PMXStandard));
    PSOCreator::Instance().setPSO(m_psoKey, cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    Matrix world = Matrix::Identity;
    if (m_transform)
        world = m_transform->getWorldMatrix();

    ModelCB modelCBData{};
    modelCBData.world = world.Transpose();
    m_modelCB->update(modelCBData);

    cmd->SetGraphicsRootDescriptorTable(1, m_modelCB->getGPUHandle());

    for (const auto& mesh : m_meshes)
    {
        mesh.vertexBuffer->bind(cmd);
        mesh.indexBuffer->bind(cmd);

        for (const auto& subset : mesh.subsets)
        {
            if (!subset.visible) continue;

            cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(subset.descriptorBase));
            cmd->SetGraphicsRootDescriptorTable(2, m_materialCB->getGPUHandle(subset.materialIndex));
            cmd->DrawIndexedInstanced(subset.indexCount, 1, subset.startIndex, 0, 0);
        }
    }
}