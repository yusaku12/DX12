#include "pch.h"
#include "GrayScaleEffect.h"

void GrayScaleEffect::initialize()
{
    registerPSO(ShaderID::GrayScalePS);
    m_constantBuffer = std::make_unique<ConstantBuffer<CBuffer>>();
}

void GrayScaleEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    //! 定数バッファ更新
    m_constantBuffer->update(m_params);

    //! PSO 設定
    applyPSO(cmd);

    //! b0: エフェクトパラメータ
    cmd->SetGraphicsRootConstantBufferView(0, m_constantBuffer->getGPUAddress());

    //! t0: 入力テクスチャ
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex));

    //! フルスクリーン描画
    drawFullscreenTriangle(cmd);
}

void GrayScaleEffect::inspectGUI()
{
    ImGui::SliderFloat("Strength", &m_params.strength, 0.0f, 1.0f);
}