#include "pch.h"
#include "TestPolygon.h"

TestPolygon::TestPolygon()
{
    Vertex vertices[] =
    {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
        {{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    };

    unsigned short indices[] =
    {
        0,1,2,
        2,1,3
    };

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        //{
        //    "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
        //    D3D12_APPEND_ALIGNED_ELEMENT,
        //    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        //},
    };

    //! 頂点バッファ生成
    m_vertexBuffer = std::make_unique<VertexBuffer<Vertex>>(vertices);

    //! インデックスバッファ生成
    m_indexBuffer = std::make_unique<IndexBuffer<unsigned short>>(indices);

    //! PSO生成
    PSOCreator::PSOData psoData;
    psoData.rootSignatureType = RootSignatureType::Standard;
    psoData.vsShaderId = ShaderID::TestPolygonVS;
    psoData.psShaderId = ShaderID::TestPolygonPS;
    psoData.rasterizerState = RasterizerState::CULL_NONE;
    psoData.blendState = BlendState::OPAQUE;
    psoData.depthStencilState = DepthStencilState::DEPTH_NONE;
    psoData.inputLayout =
    {
        inputLayout[0],
        //inputLayout[1],
    };
    m_psoCreator = std::make_unique<PSOCreator>(psoData);
}

void TestPolygon::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    //! PSO設定
    m_psoCreator->setPSO();

    //! プリミティブトポロジー設定
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //! バッファバインド
    m_vertexBuffer->bind();
    m_indexBuffer->bind();

    //! 描画
    cmd->DrawIndexedInstanced(6, 1, 0, 0, 0);
}