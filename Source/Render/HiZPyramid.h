#pragma once

//! Hi-Z depth pyramid generation (GPU compute)
class HiZPyramid
{
public:
    static HiZPyramid& Instance()
    {
        static HiZPyramid instance;
        return instance;
    }

    //! Build Hi-Z from depth
    void build(ID3D12GraphicsCommandList* cmd);

    //! Top-level SRV index (mip0)
    UINT getSrvIndex() const { return m_mipSrvIndices.empty() ? UINT_MAX : m_mipSrvIndices.front(); }

    //! Enable/disable
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    //! Debug UI
    void debugImgui();

private:
    HiZPyramid() = default;

    struct HiZParams
    {
        UINT srcWidth = 1;
        UINT srcHeight = 1;
        UINT isFirstPass = 0;
        UINT padding = 0;
    };

    void ensureInitialized();
    void recreateResourcesIfNeeded();
    void releaseDescriptors();

    bool m_enabled = true;
    bool m_initialized = false;

    UINT m_width = 0;
    UINT m_height = 0;
    UINT m_mipCount = 0;

    std::unique_ptr<ConstantBuffer<HiZParams>> m_paramsCB;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_hizTexture;

    std::vector<UINT> m_mipSrvIndices;
    std::vector<UINT> m_mipUavIndices;

    UINT m_lastDispatchCount = 0;
};
