#pragma once

#include <ffx_api.hpp>

//=====================================================
//! AMD FidelityFX Super Resolution Upscaler
//=====================================================
class FidelityFXUpscaler
{
public:

    static FidelityFXUpscaler& Instance()
    {
        static FidelityFXUpscaler instance;
        return instance;
    }

    void initialize();
    void shutdown();
    void beginFrame();
    UINT execute(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex);
    void renderDebugContents();

    bool isEnabled() const { return m_enabled && m_context != nullptr; }
    Vector2 getJitter() const { return isEnabled() ? m_jitter : Vector2::Zero; }

private:

    FidelityFXUpscaler() = default;

    bool createContext();
    void destroyContext();
    bool ensureOutput(UINT width, UINT height);
    void applyQualityMode();

    ffx::Context m_context = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_output;
    D3D12_RESOURCE_STATES m_outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    UINT m_outputSrvIndex = UINT_MAX;
    UINT m_outputWidth = 0;
    UINT m_outputHeight = 0;
    UINT m_previousRenderWidth = 0;
    UINT m_previousRenderHeight = 0;
    Vector2 m_jitter = Vector2::Zero;
    int32_t m_jitterIndex = 0;
    int32_t m_jitterPhaseCount = 1;
    int m_qualityMode = 1;
    bool m_enabled = false;
    bool m_enableSharpening = true;
    float m_sharpness = 0.35f;
};