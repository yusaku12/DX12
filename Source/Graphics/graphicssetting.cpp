#include "pch.h"
#include "graphicssetting.h"

GraphicsSetting::GraphicsSetting()
{
}

void GraphicsSetting::bindRootSignature(UINT rootParameterIndex)
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    //! DescriptorHeap 内の GPU ハンドルを取得
    auto gpuHandle = DX12::Instance().getGpuHandle(m_cbvHandle);

    //! Root Signature にバインドする
    cmd->SetGraphicsRootDescriptorTable(rootParameterIndex, gpuHandle);
}

void GraphicsSetting::setMeshBuffers(D3D12_PRIMITIVE_TOPOLOGY topology)
{
    const auto& cmd = DX12::Instance().getGraphicsCommandList();

    cmd->IASetPrimitiveTopology(topology);
    cmd->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    cmd->IASetIndexBuffer(&m_indexBufferView);
}