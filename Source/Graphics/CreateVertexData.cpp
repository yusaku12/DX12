#include "pch.h"
#include "CreateVertexData.h"

void CreateVertexData::bindVertexBuffer()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();
    cmd->IASetVertexBuffers(0, 1, &m_vertexBufferView);
}