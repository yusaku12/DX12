#include "pch.h"
#include "PSOCreator.h"

size_t PSOCreator::registerPSO(const PSOData& data)
{
    size_t key = data.computeHash();

    // 既にキャッシュにあればそのまま返す
    if (m_cache.contains(key))
        return key;

    // PSO生成してキャッシュに登録
    CacheEntry entry;
    entry.data = data;
    entry.pipelineState = buildPSO(data);
    m_cache[key] = std::move(entry);

    return key;
}

void PSOCreator::setPSO(size_t key)
{
    auto it = m_cache.find(key);
    if (it == m_cache.end())
    {
        assert(false && "PSOCreator::setPSO - key not found");
        return;
    }

    auto cmd = DX12::Instance().getGraphicsCommandList();
    const auto& entry = it->second;

    cmd->SetGraphicsRootSignature(
        RootSignatureManager::Instance().getRootSignature(entry.data.rootSignatureType));
    cmd->SetPipelineState(entry.pipelineState.Get());
}

void PSOCreator::setPSO(size_t key, ID3D12GraphicsCommandList* cmd)
{
    auto it = m_cache.find(key);
    if (it == m_cache.end())
    {
        assert(false && "PSOCreator::setPSO - key not found");
        return;
    }

    const auto& entry = it->second;

    cmd->SetGraphicsRootSignature(
        RootSignatureManager::Instance().getRootSignature(entry.data.rootSignatureType));
    cmd->SetPipelineState(entry.pipelineState.Get());
}

void PSOCreator::refreshDirtyPSOs()
{
    for (auto& [key, entry] : m_cache)
    {
        bool vsDirty = ShaderManager::Instance().isDirty(entry.data.vsShaderId);
        bool psDirty = ShaderManager::Instance().isDirty(entry.data.psShaderId);

        if (vsDirty || psDirty)
        {
            // PSO再構築
            entry.pipelineState = buildPSO(entry.data);

            if (vsDirty)
                ShaderManager::Instance().clearDirty(entry.data.vsShaderId);
            if (psDirty)
                ShaderManager::Instance().clearDirty(entry.data.psShaderId);
        }
    }
}

void PSOCreator::clearAll()
{
    m_cache.clear();
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOCreator::buildPSO(const PSOData& data)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

    // ルートシグネチャ設定
    gpipeline.pRootSignature = RootSignatureManager::Instance().getRootSignature(data.rootSignatureType);

    // シェーダ設定
    gpipeline.VS.pShaderBytecode = ShaderManager::Instance().getShaderBlob(data.vsShaderId)->GetBufferPointer();
    gpipeline.VS.BytecodeLength = ShaderManager::Instance().getShaderBlob(data.vsShaderId)->GetBufferSize();
    gpipeline.PS.pShaderBytecode = ShaderManager::Instance().getShaderBlob(data.psShaderId)->GetBufferPointer();
    gpipeline.PS.BytecodeLength = ShaderManager::Instance().getShaderBlob(data.psShaderId)->GetBufferSize();

    // ラスタライザステート設定
    gpipeline.RasterizerState = PiplineState::Instance().getRasterizerState(data.rasterizerState);

    // ブレンドステート設定
    gpipeline.BlendState = PiplineState::Instance().getBlendState(data.blendState);

    // デプスステンシルステート設定
    gpipeline.DepthStencilState = PiplineState::Instance().getDepthStencilState(data.depthStencilState);

    // 入力レイアウト設定
    gpipeline.InputLayout.pInputElementDescs = data.inputLayout.data();
    gpipeline.InputLayout.NumElements = (UINT)data.inputLayout.size();

    // その他設定
    gpipeline.SampleMask = UINT_MAX;
    gpipeline.PrimitiveTopologyType = data.topologyType;
    gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    gpipeline.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // ここのコードは改善必須
    {
        // レンダーターゲット設定
        gpipeline.NumRenderTargets = 1;
        gpipeline.RTVFormats[0] = DX12::Instance().getBackBufferFormat();

        // サンプル設定
        gpipeline.SampleDesc.Count = 1;
        gpipeline.SampleDesc.Quality = 0;
    }

    // パイプラインステート作成
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = DX12::Instance().getDevice()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(pso.GetAddressOf()));
    assert(SUCCEEDED(hr));

    return pso;
}