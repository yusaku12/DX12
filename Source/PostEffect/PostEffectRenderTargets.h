#pragma once

//=====================================================
//! ポストエフェクト用ピンポンレンダーターゲット管理
//=====================================================
class PostEffectRenderTargets
{
public:

    static PostEffectRenderTargets& Instance()
    {
        static PostEffectRenderTargets instance;
        return instance;
    }

    //! 初期化（DX12::initialize の後に呼ぶ）
    void initialize();

    //! リサイズ対応
    void resize(UINT width, UINT height);

    //! 現在の書き込み先 RTV ハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRTV() const;

    //! 現在の入力 SRV インデックス
    UINT getCurrentInputSrvIndex() const { return m_inputSrvIndex; }

    //! 書き込み先を RENDER_TARGET に遷移
    void transitionWriteToRenderTarget(ID3D12GraphicsCommandList* cmd);

    //! 書き込み先を SRV に遷移
    void transitionWriteToSRV(ID3D12GraphicsCommandList* cmd);

    //! ピンポンをスワップ（書き込み先 → 次の入力）
    void swap();

    //! フレーム開始時リセット
    void reset(UINT sceneSrvIndex);

    //! 最終出力 SRV インデックス
    UINT getFinalOutputSrvIndex() const { return m_inputSrvIndex; }

private:

    PostEffectRenderTargets() = default;

    static constexpr int PING_PONG_COUNT = 2;

    //! リソース作成の内部実装
    void createResources(UINT width, UINT height, DXGI_FORMAT format);

    //! リソース解放
    void releaseResources();

    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[PING_PONG_COUNT];
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandles[PING_PONG_COUNT]{};
    UINT m_srvIndices[PING_PONG_COUNT]{ UINT_MAX, UINT_MAX };
    D3D12_RESOURCE_STATES m_states[PING_PONG_COUNT]{
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    };

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;

    int m_writeIndex = 0;
    UINT m_inputSrvIndex = 0;
    bool m_initialized = false;
};