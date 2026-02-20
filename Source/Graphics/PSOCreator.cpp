#include "pch.h"
#include "PSOCreator.h"

PSOCreator::PSOCreator(const PSOData& data)
{
    //! PSOデータ保存
    m_psoData = data;

    //! PSO作成
    createPSO();
}

void PSOCreator::createPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

    //! ルートシグネチャ設定
    gpipeline.pRootSignature = RootSignatureManager::Instance().getRootSignature(m_psoData.rootSignatureType);

    //! シェーダ設定(シェーダが増えた際の対応を考える必要がある)
    gpipeline.VS.pShaderBytecode = ShaderManager::Instance().getShaderBlob(m_psoData.vsShaderId)->GetBufferPointer();
    gpipeline.VS.BytecodeLength = ShaderManager::Instance().getShaderBlob(m_psoData.vsShaderId)->GetBufferSize();
    gpipeline.PS.pShaderBytecode = ShaderManager::Instance().getShaderBlob(m_psoData.psShaderId)->GetBufferPointer();
    gpipeline.PS.BytecodeLength = ShaderManager::Instance().getShaderBlob(m_psoData.psShaderId)->GetBufferSize();

    //! ラスタライザステート設定
    gpipeline.RasterizerState = PiplineState::Instance().getRasterizerState(m_psoData.rasterizerState);

    //! ブレンドステート設定
    gpipeline.BlendState = PiplineState::Instance().getBlendState(m_psoData.blendState);

    //! デプスステンシルステート設定
    gpipeline.DepthStencilState = PiplineState::Instance().getDepthStencilState(m_psoData.depthStencilState);

    //! 入力レイアウト設定
    gpipeline.InputLayout.pInputElementDescs = m_psoData.inputLayout.data();
    gpipeline.InputLayout.NumElements = (UINT)m_psoData.inputLayout.size();

    //! その他設定
    gpipeline.SampleMask = UINT_MAX;
    gpipeline.PrimitiveTopologyType = m_psoData.topologyType;
    gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    gpipeline.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // ここのコードは改善必須
    {
        //! レンダーターゲット設定
        gpipeline.NumRenderTargets = 1;
        gpipeline.RTVFormats[0] = DX12::Instance().getBackBufferFormat();

        //! 入力レイアウト設定
        gpipeline.SampleDesc.Count = 1;
        gpipeline.SampleDesc.Quality = 0;
    }

    //! パイプラインステート作成
    DX12::Instance().getDevice()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(m_pPipelineState.GetAddressOf()));
}

void PSOCreator::setPSO()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(m_psoData.rootSignatureType));
    cmd->SetPipelineState(m_pPipelineState.Get());
}