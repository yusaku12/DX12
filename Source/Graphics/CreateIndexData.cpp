#include "pch.h"
#include "CreateIndexData.h"

void CreateIndexData::bindIndexBuffer()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();
    cmd->IASetIndexBuffer(&m_indexBufferView);
}