#include "pch.h"
#include "graphicssetting.h"

GraphicsSetting::GraphicsSetting()
    :m_mappedCB(nullptr)
{
}

GraphicsSetting::~GraphicsSetting()
{
    //! 常に Unmap する（アプリ終了時など）
    if (m_constantBuffer && m_mappedCB)
    {
        m_constantBuffer->Unmap(0, nullptr);
        m_mappedCB = nullptr;
    }
}

void GraphicsSetting::bindConstantBuffer(UINT rootParameterIndex, bool rootIsDescriptorTable)
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    if (rootIsDescriptorTable)
    {
        //! CBV ディスクリプタテーブルをセット
        cmd->SetGraphicsRootDescriptorTable(rootParameterIndex, m_cbvHandleGPU);
    }
    else
    {
        //! 直接 CBV をセット
        cmd->SetGraphicsRootConstantBufferView(rootParameterIndex, m_constantBuffer->GetGPUVirtualAddress());
    }
}

void GraphicsSetting::setMeshBuffers(D3D12_PRIMITIVE_TOPOLOGY topology)
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    //! トポロジー
    cmd->IASetPrimitiveTopology(topology);

    //! VB
    cmd->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    //! IB
    cmd->IASetIndexBuffer(&m_indexBufferView);
}