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

    //! 頂点バッファ生成
    m_vertexBuffer = std::make_unique<VertexBuffer<Vertex>>(vertices);

    //! インデックスバッファ生成
    m_indexBuffer = std::make_unique<IndexBuffer<unsigned short>>(indices);
}

void TestPolygon::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    //! プリミティブトポロジー設定
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //! バッファバインド
    m_vertexBuffer->bind();
    m_indexBuffer->bind();

    //! 描画
    cmd->DrawIndexedInstanced(6, 1, 0, 0, 0);
}