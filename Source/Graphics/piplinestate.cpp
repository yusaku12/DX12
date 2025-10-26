#include "pch.h"

const D3D12_STATIC_SAMPLER_DESC& setSamplerState(SamplerState samplerState, D3D12_SHADER_VISIBILITY shderType, int shderSlot, int spaceSlot)
{
    D3D12_STATIC_SAMPLER_DESC samplerDesc = {};

    //! サンプラーステートの各設定
    switch (samplerState)
    {
    case SamplerState::POINT_WRAP:
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        break;
    case SamplerState::POINT_CLAMP:
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        break;
    case SamplerState::LINEAR_WRAP:
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        break;
    case SamplerState::LINEAR_CLAMP:
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        break;
    case SamplerState::ANISOTROPIC_WRAP:
        samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        break;
    case SamplerState::ANISOTROPIC_CLAMP:
        samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        break;
    }

    //! 共通設定
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.ShaderRegister = shderSlot;   //!< 使うシェーダのレジスター番号
    samplerDesc.RegisterSpace = spaceSlot;    //!< スロット指定
    samplerDesc.ShaderVisibility = shderType; //!< 使うシェーダを設定

    return samplerDesc;
}

D3D12_BLEND_DESC setBlendState(BlendState blendState)
{
    D3D12_BLEND_DESC blendDesc = {};

    //! ブレンドステート設定
    switch (blendState)
    {
    case BlendState::OPAQUE:
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = FALSE;
        blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        break;
    case BlendState::ALPHA:
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        break;
    case BlendState::ADD:
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        break;
    case BlendState::MULTIPLIE:
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        break;
    }

    //! 共通設定
    blendDesc.IndependentBlendEnable = FALSE;

    return blendDesc;
}

D3D12_DEPTH_STENCIL_DESC setDepthStencilState(DepthStencilState depthStencilState)
{
    D3D12_DEPTH_STENCIL_DESC depthDesc = {};

    //! デプスステンシルステート設定
    switch (depthStencilState)
    {
    case DepthStencilState::DEPTH_NONE:
        depthDesc.DepthEnable = FALSE;
        depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        break;
    case DepthStencilState::DEPTH_DEFALT:
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        break;
    case DepthStencilState::DEPTH_READ:
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        break;
    case DepthStencilState::DEPTH_REVERSE_Z:
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        break;
    case DepthStencilState::DEPTH_READ_REVERSE_Z:
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        break;
    }

    //! 共通設定
    depthDesc.StencilEnable = FALSE;
    depthDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    depthDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depthDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    depthDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depthDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depthDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depthDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    depthDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depthDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    return depthDesc;
}

D3D12_RASTERIZER_DESC setRasterizerState(RasterizerState rasterizerState)
{
    D3D12_RASTERIZER_DESC rasterizerDesc = {};

    //! ラスタライザステート設定
    switch (rasterizerState)
    {
    case RasterizerState::CULL_NONE:
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
        break;
    case RasterizerState::CULL_CLOCKWISE:
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;
        break;
    case RasterizerState::CULL_COUNTER_CLOCKWISE:
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        break;
    case RasterizerState::WIRE_FRAME:
        rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
        break;
    }

    //! 共通設定
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = TRUE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    return rasterizerDesc;
}

void setPlpelineStateObject(D3D12_GRAPHICS_PIPELINE_STATE_DESC* psoDesc, BlendState blendState, DepthStencilState depthStencilState, RasterizerState rasterizerState)
{
    psoDesc->BlendState = setBlendState(blendState);
    psoDesc->RasterizerState = setRasterizerState(rasterizerState);
    psoDesc->DepthStencilState = setDepthStencilState(depthStencilState);
}