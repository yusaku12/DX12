#pragma once

#include "PostEffectBase.h"

class CameraComponent;

//=====================================================
//! Temporal Anti-Aliasing
//! 履歴再投影 + 近傍クランプでジャギーとシマーを抑える
//=====================================================
class TemporalAAEffect : public PostEffectBase
{
public:

    TemporalAAEffect() { m_priority = 85; }
    ~TemporalAAEffect() override;

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName() const override { return "TemporalAA"; }
    ShaderID getPixelShaderID() const override { return ShaderID::TemporalAAPS; }
    bool needsDepth() const override { return false; }

private:

    struct CBuffer
    {
        Matrix currentViewProj{};
        Matrix prevViewProj{};
        Matrix invViewProj{};
        Vector4 blendParams{};    //!< x=stationary y=motion yScale z=historyValid
        Vector4 texelParams{};    //!< x=1/width y=1/height z=jitterX w=jitterY
        Vector4 prevJitter{};     //!< x=prevJitterX y=prevJitterY
    };

    void ensureHistoryResources(UINT width, UINT height);
    void releaseHistoryResources();

    void transitionHistoryToSRV(ID3D12GraphicsCommandList* cmd, int index);
    void transitionHistoryToCopyDest(ID3D12GraphicsCommandList* cmd, int index);

    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_history[2];
    D3D12_RESOURCE_STATES m_historyState[2]{
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    };
    UINT m_historySrv[2]{ UINT_MAX, UINT_MAX };

    Matrix m_prevViewProj = Matrix::Identity;
    bool m_hasPrevViewProj = false;

    Vector3 m_prevCamPos = Vector3::Zero;
    Vector3 m_prevCamForward = Vector3::Forward;

    UINT m_width = 0;
    UINT m_height = 0;

    int m_historyReadIndex = 0;
    int m_historyWriteIndex = 1;
    bool m_hasHistory = false;

    float m_stationaryBlend = 0.92f;
    float m_motionBlend = 0.12f;
    float m_motionScale = 140.0f;
    float m_cameraCutPositionThreshold = 4.0f;
    float m_cameraCutAngleThresholdDeg = 35.0f;
};
