#include "pch.h"
#include "PostEffectBase.h"

void PostEffectBase::registerPSO(ShaderID psShaderID)
{
    PSOCreator::PSOData data{};
    data.rootSignatureType = RootSignatureType::PostEffect;
    data.vsShaderId = ShaderID::PostEffectVS;
    data.psShaderId = psShaderID;
    data.rasterizerState = RasterizerState::CULL_NONE;
    data.blendState = BlendState::OPAQUE;
    data.depthStencilState = DepthStencilState::DEPTH_NONE;
    data.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // 入力レイアウトなし（VS で SV_VertexID から生成）

    m_psoKey = PSOCreator::Instance().registerPSO(data);
}

void PostEffectBase::drawFullscreenTriangle(ID3D12GraphicsCommandList* cmd)
{
    // 頂点バッファなしのフルスクリーン三角形
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->IASetIndexBuffer(nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);
}

void PostEffectBase::applyPSO(ID3D12GraphicsCommandList* cmd)
{
    PSOCreator::Instance().setPSO(m_psoKey, cmd);
}