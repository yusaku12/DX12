#pragma once

//=====================================================
//! ブレンドステート
//=====================================================
enum class BlendState : int
{
    OPAQUE,
    ALPHA,
    ADD,
    MULTIPLIE,
    MAX
};

//=====================================================
//! デプスステンシルステート
//=====================================================
enum class DepthStencilState : int
{
    DEPTH_NONE,
    DEPTH_DEFALT,
    DEPTH_READ,
    DEPTH_REVERSE_Z,
    DEPTH_READ_REVERSE_Z,
    MAX
};

//=====================================================
//! ラスタライザステート
//=====================================================
enum class RasterizerState : int
{
    CULL_NONE,
    CULL_CLOCKWISE,
    CULL_COUNTER_CLOCKWISE,
    WIRE_FRAME,
    MAX
};

//=====================================================
//! サンプラーステート
//=====================================================
enum class SamplerState : int
{
    POINT_WRAP,
    POINT_CLAMP,
    LINEAR_WRAP,
    LINEAR_CLAMP,
    ANISOTROPIC_WRAP,
    ANISOTROPIC_CLAMP,
    MAX
};

//=====================================================
// パイプラインステート管理シングルトン
//=====================================================
class PiplineState
{
public:

    //! インスタンス取得
    static PiplineState& Instance()
    {
        static PiplineState instance;
        return instance;
    }

    //! 初期化
    void initialize();

    //! 各種パイプラインステート取得
    const D3D12_STATIC_SAMPLER_DESC& getSamplerState(SamplerState state) const { return m_samplerState[static_cast<int>(state)]; };
    const D3D12_STATIC_SAMPLER_DESC* getSamplerStates() const { return m_samplerState; }
    const D3D12_BLEND_DESC& getBlendState(BlendState state) const { return m_blendState[static_cast<int>(state)]; };
    const D3D12_DEPTH_STENCIL_DESC& getDepthStencilState(DepthStencilState state) const { return m_depthStencilState[static_cast<int>(state)]; };
    const D3D12_RASTERIZER_DESC& getRasterizerState(RasterizerState state) const { return m_rasterizerState[static_cast<int>(state)]; };

private:

    PiplineState() = default;
    ~PiplineState() = default;

    //! サンプラーステート初期化
    void initSamplerState();

    //! ブレンドステート初期化
    void initBlendState();

    //! デプスステンシルステート初期化
    void initDepthStencilState();

    //! ラスタライザステート初期化
    void initRasterizerState();

    //! 各種パイプラインステート
    D3D12_STATIC_SAMPLER_DESC m_samplerState[static_cast<int>(SamplerState::MAX)] = {};
    D3D12_BLEND_DESC m_blendState[static_cast<int>(BlendState::MAX)] = {};
    D3D12_DEPTH_STENCIL_DESC m_depthStencilState[static_cast<int>(DepthStencilState::MAX)] = {};
    D3D12_RASTERIZER_DESC m_rasterizerState[static_cast<int>(RasterizerState::MAX)] = {};
};