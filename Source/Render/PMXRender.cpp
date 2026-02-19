#include "pch.h"
#include "PMXRender.h"

PMXRender::PMXRender(const std::wstring& filePath)
{
    //! PMXファイルの読み込み
    m_pmxLoad = std::make_unique<PmxLoad>(filePath, m_pmxFileData);

    //! 頂点バッファの作成
    createVertexBuffer();

    //! インデックスバッファの作成
    createIndexBuffer();

    //! マテリアルCBVの作成
    createMaterialCBV();

    //! サブセットの作成
    createSubsets();

    //! PSOの作成
    createPSO();
}

void PMXRender::createVertexBuffer()
{
    std::vector<Vertex> vertices;

    for (const auto& v : m_pmxFileData.vertices)
    {
        Vertex gv{};
        gv.position = v.position;
        gv.normal = v.normal;
        gv.uv = v.uv;

        for (int i = 0; i < 4; ++i)
        {
            gv.boneIndex[i] = v.boneIndices[i];
            gv.boneWeight[i] = v.boneWeights[i];
        }

        vertices.push_back(gv);
    }

    //! 頂点バッファの作成
    m_vertexBuffer = std::make_unique<VertexBuffer<Vertex>>(vertices);
}

void PMXRender::createIndexBuffer()
{
    std::vector<uint32_t> indices;

    for (const auto& f : m_pmxFileData.faces)
    {
        indices.push_back(f.vertices[0]);
        indices.push_back(f.vertices[1]);
        indices.push_back(f.vertices[2]);
    }

    //! インデックスバッファの作成
    m_indexBuffer = std::make_unique<IndexBuffer<unsigned int>>(indices);
}

void PMXRender::createMaterialCBV()
{
    UINT materialCount = (UINT)m_pmxFileData.materials.size();

    //! マテリアルCBVの作成
    m_materialCB = std::make_unique<ConstantBuffer<Material>>(materialCount);

    for (UINT i = 0; i < materialCount; ++i)
    {
        const auto& m = m_pmxFileData.materials[i];
        Material material{};
        material.diffuse = m.diffuse;
        material.specular = m.specular;
        material.specularPower = m.specularPower;
        material.ambient = m.ambient;
        m_materialCB->update(material, i);
    }
}

void PMXRender::createSubsets()
{
    // マテリアルごとの描画単位を作成
    UINT start = 0;
    for (size_t i = 0; i < m_pmxFileData.materials.size(); ++i)
    {
        Subset s{};
        s.startIndex = start;
        s.indexCount = m_pmxFileData.materials[i].numFaceVertices;
        s.materialIndex = (UINT)i;

        start += s.indexCount;

        m_subsets.push_back(s);
    }
}

void PMXRender::createPSO()
{
    //PSOCreator::PSOData psoData{};
    //psoData.rootSignatureType = RootSignatureType::Standard;
    //psoData.vsShaderId = ShaderID::PMXVS;
    //psoData.psShaderId = ShaderID::PMXPS;
    //psoData.rasterizerState = RasterizerState::CULL_BACK;
    //psoData.blendState = BlendState::OPAQUE;
    //psoData.depthStencilState = DepthStencilState::DEPTH_DEFAULT;
    //psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    //psoData.inputLayout =
    //{
    //    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    //    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    //    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    //    { "BONEINDEX", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    //    { "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    //};
    //m_psoCreator = std::make_unique<PSOCreator>(psoData);
}

void PMXRender::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    //! DescriptorHeap
    DescriptorHeapManager::Instance().setDiscriptorHeap();

    //! RootSignature
    //cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::Standard));

    //! PSO
    //m_psoCreator->setPSO();

    //! IA
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //! VB/IB
    m_vertexBuffer->bind();
    m_indexBuffer->bind();

    //! CBV(カメラ)
    //cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    for (const auto& subset : m_subsets)
    {
        cmd->DrawIndexedInstanced(subset.indexCount, 1, subset.startIndex, 0, 0);
    }
}