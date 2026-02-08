#include "pch.h"

void PSOManager::createPSO(const PSOData& psoData)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

    //! ルートシグネチャ設定
    gpipeline.pRootSignature = RootSignatureManager::Instance().getRootSignature(psoData.rootSignatureType);

    //! シェーダ設定(シェーダが増えた際の対応を考える必要がある)
    gpipeline.VS.pShaderBytecode = ShaderManager::Instance().getShaderBlob(psoData.vsShaderId)->GetBufferPointer();
    gpipeline.VS.BytecodeLength = ShaderManager::Instance().getShaderBlob(psoData.vsShaderId)->GetBufferSize();
    gpipeline.PS.pShaderBytecode = ShaderManager::Instance().getShaderBlob(psoData.psShaderId)->GetBufferPointer();
    gpipeline.PS.BytecodeLength = ShaderManager::Instance().getShaderBlob(psoData.psShaderId)->GetBufferSize();

    //! ラスタライザステート設定
    gpipeline.RasterizerState = PiplineState::Instance().getRasterizerState(psoData.rasterizerState);

    //! ブレンドステート設定
    gpipeline.BlendState = PiplineState::Instance().getBlendState(psoData.blendState);

    //! デプスステンシルステート設定
    gpipeline.DepthStencilState = PiplineState::Instance().getDepthStencilState(psoData.depthStencilState);

    //! パイプラインステート作成
    DX12::Instance().getDevice()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(m_pPipelineState.GetAddressOf()));
}

void PSOManager::setPSO()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    cmd->SetPipelineState(m_pPipelineState.Get());
}