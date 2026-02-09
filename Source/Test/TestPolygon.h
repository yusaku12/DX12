#pragma once

#include "Graphics\VertexBuffer.h"
#include "Graphics\IndexBuffer.h"
#include "Graphics\PSOCreator.h"

class TestPolygon
{
public:

    explicit TestPolygon();
    ~TestPolygon() {}

    //! 描画
    void render();

private:

    struct Vertex
    {
        Vector3 pos = {};
        Vector2 uv = {};
    };

    std::unique_ptr<VertexBuffer<Vertex>>m_vertexBuffer;
    std::unique_ptr<IndexBuffer<unsigned short>> m_indexBuffer;
    std::unique_ptr<PSOCreator>m_psoCreator;
    LoadTexture* m_loadTexture = nullptr;
};