#pragma once

//! ブレンドステート
enum class BlendState
{
    OPAQUE,
    ALPHA,
    ADD,
    MULTIPLIE
};

//! デプスステンシルステート
enum class DepthStencilState
{
    DEPTH_NONE,
    DEPTH_DEFALT,
    DEPTH_READ,
    DEPTH_REVERSE_Z,
    DEPTH_READ_REVERSE_Z
};

//! ラスタライザステート
enum class RasterizerState
{
    CULL_NONE,
    CULL_CLOCKWISE,
    CULL_COUNTER_CLOCKWISE,
    WIRE_FRAME
};

//! サンプラーステート
enum class SamplerState
{
    POINT_WRAP,
    POINT_CLAMP,
    LINEAR_WRAP,
    LINEAR_CLAMP,
    ANISOTROPIC_WRAP,
    ANISOTROPIC_CLAMP
};

//! サンプラーステート設定
const D3D12_STATIC_SAMPLER_DESC& setSamplerState(SamplerState samplerState, D3D12_SHADER_VISIBILITY shderType, int shderSlot, int spaceSlot);

//! ブレンドステート設定
D3D12_BLEND_DESC setBlendState(BlendState blendState);

//! デプスステンシルステート設定
D3D12_DEPTH_STENCIL_DESC setDepthStencilState(DepthStencilState depthStencilState);

//! ラスタライザステート設定
D3D12_RASTERIZER_DESC setRasterizerState(RasterizerState rasterizerState);

//! パイプラインの設定
void setPlpelineStateObject(D3D12_GRAPHICS_PIPELINE_STATE_DESC* psoDesc, BlendState blendState, DepthStencilState depthStencilState, RasterizerState rasterizerState);
