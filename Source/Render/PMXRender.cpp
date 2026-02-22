#include "pch.h"
#include "PMXRender.h"

PMXRender::PMXRender(const std::wstring& filePath)
{
    //! PMXファイルの読み込み
    m_pmxLoad = std::make_unique<PmxLoad>(filePath, m_pmxFileData);

    //! モデルCBVの作成
    m_modelCB = std::make_unique<ConstantBuffer<Model>>();
    m_modelCB->update(Model{});

    //! テクスチャの読み込み
    createTextures();

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

        //for (int i = 0; i < 4; ++i)
        //{
        //    gv.boneIndex[i] = v.boneIndices[i];
        //    gv.boneWeight[i] = v.boneWeights[i];
        //}

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
        //material.specularPower = m.specularPower;
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
        s.textureIndex = m_pmxFileData.materials[i].textureIndex;
        s.visible = true;

        start += s.indexCount;

        m_subsets.push_back(s);
    }
}

void PMXRender::createTextures()
{
    for (auto texName : m_pmxFileData.textures)
    {
        std::wstring fullPath = L"Data/Texture/dummyWhite.jpg";
        auto texture = TextureManager::Instance().load(fullPath);

        m_textures.push_back(texture);
        m_texturePaths.push_back(fullPath);
    }
}

void PMXRender::createPSO()
{
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::PMXStandard;
    psoData.vsShaderId = ShaderID::PMXVS;
    psoData.psShaderId = ShaderID::PMXPS;
    psoData.rasterizerState = RasterizerState::CULL_CLOCKWISE;
    psoData.blendState = BlendState::ALPHA;
    psoData.depthStencilState = DepthStencilState::DEPTH_DEFALT;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //{ "BONEINDEX", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //{ "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_psoCreator = std::make_unique<PSOCreator>(psoData);
}

void PMXRender::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    //! DescriptorHeap
    DescriptorHeapManager::Instance().setDiscriptorHeap();

    //! RootSignature
    cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::PMXStandard));

    //! PSO
    m_psoCreator->setPSO();

    //! IA
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //! VB/IB
    m_vertexBuffer->bind();
    m_indexBuffer->bind();

    //! CBV(カメラ)
    cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    //! DescriptorTable(モデル行列)
    cmd->SetGraphicsRootDescriptorTable(2, m_modelCB->getGPUHandle());

    for (const auto& subset : m_subsets)
    {
        //! DescriptorTable(テクスチャ)
        if (subset.textureIndex >= 0 && subset.textureIndex < m_textures.size())
        {
            cmd->SetGraphicsRootDescriptorTable(1, m_textures[subset.textureIndex]->getGPUHandle());
        }

        //! DescriptorTable(マテリアル)
        cmd->SetGraphicsRootDescriptorTable(3, m_materialCB->getGPUHandle(subset.materialIndex));

        //! Draw
        if (subset.visible)
        {
            cmd->DrawIndexedInstanced(subset.indexCount, 1, subset.startIndex, 0, 0);
        }
    }
}

void PMXRender::debugRender()
{
    if (ImGui::Begin("PMX Editor"))
    {
        for (size_t i = 0; i < m_subsets.size(); ++i)
        {
            auto& subset = m_subsets[i];
            const auto& pmxMat = m_pmxFileData.materials[i];

            ImGui::Separator();

            //! マテリアル名
            std::string matName = std::string(pmxMat.name.begin(), pmxMat.name.end());
            ImGui::Text("Name : %s", matName.c_str());

            ImGui::Checkbox(("Visible##" + std::to_string(i)).c_str(), &subset.visible);

            //! テクスチャプレビュー
            if (subset.textureIndex >= 0 && subset.textureIndex < m_textures.size())
            {
                ImTextureID texID = (ImTextureID)m_textures[subset.textureIndex]->getGPUHandle().ptr;
                ImGui::Image(texID, ImVec2(100, 100));
            }

            //! テクスチャ変更ボタン
            std::string buttonLabel = "Change Texture##" + std::to_string(i);

            if (ImGui::Button(buttonLabel.c_str()))
            {
                std::vector<std::wstring> paths;

                if (Dialog::openFile(
                    paths,
                    L"テクスチャを選択",
                    L"Data/Model",
                    false) == DialogResult::OK)
                {
                    std::wstring newPath = paths[0];

                    //! テクスチャロード
                    auto newTexture = TextureManager::Instance().load(newPath);

                    if (newTexture)
                    {
                        //! 既存テクスチャ差し替え
                        subset.textureIndex = (int)m_textures.size();

                        m_textures.push_back(newTexture);
                        m_texturePaths.push_back(newPath);
                    }
                }
            }
        }
    }

    ImGui::End();
}