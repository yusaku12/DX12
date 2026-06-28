#pragma once

//=====================================================
//! GBuffer レンダーターゲット管理
//=====================================================
class GBufferRenderTargets
{
public:

    static constexpr UINT RenderTargetCount = 3;
    static constexpr DXGI_FORMAT BaseColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT NormalRoughnessFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT WorldPosAoFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    static GBufferRenderTargets& Instance()
    {
        static GBufferRenderTargets instance;
        return instance;
    }

    //! 初期化（DX12::initialize の後に呼ぶ）
    void initialize();

    //! リサイズ対応
    void resize(UINT width, UINT height);

    //! SRV ベースインデックス（t0 から連続）
    UINT getSrvBaseIndex() const { return m_srvBaseIndex; }

    //! RTV ハンドル配列取得
    const D3D12_CPU_DESCRIPTOR_HANDLE* getRTVHandles() const { return m_rtvHandles; }

    //! 書き込み先を RENDER_TARGET に遷移
    void transitionToRenderTarget(ID3D12GraphicsCommandList* cmd);

    //! 書き込み先を SRV に遷移
    void transitionToSRV(ID3D12GraphicsCommandList* cmd);

    //! クリア
    void clear(ID3D12GraphicsCommandList* cmd);

    //! OMSetRenderTargets 用
    void setRenderTargets(ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const;

    //! デバッグ描画（ImGui）
    void debugDrawImGui();

    //! デバッグ中身表示
    void renderDebugContents();

    //! SRV インデックス取得
    UINT getSrvIndex(UINT index) const
    {
        if (m_srvBaseIndex == UINT_MAX || index >= RenderTargetCount)
            return UINT_MAX;

        return m_srvBaseIndex + index;
    }

private:

    GBufferRenderTargets() = default;

    //! リソース作成の内部実装
    void createResources(UINT width, UINT height);

    //! リソース解放
    void releaseResources();

    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[RenderTargetCount];
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandles[RenderTargetCount]{};
    D3D12_RESOURCE_STATES m_states[RenderTargetCount]{
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    };

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    UINT m_srvBaseIndex = UINT_MAX;
    bool m_initialized = false;
};