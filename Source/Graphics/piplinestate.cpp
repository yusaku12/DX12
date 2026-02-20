#include "pch.h"

void PiplineState::initialize()
{
    //! サンプラーステート初期化
    initSamplerState();

    //! ブレンドステート初期化
    initBlendState();

    //! デプスステンシルステート初期化
    initDepthStencilState();

    //! ラスタライザステート初期化
    initRasterizerState();
}

void PiplineState::initSamplerState()
{
    //! サンプラーステートの基本設定を作成するラムダ
    auto makeSamplerDesc = [&]()
        {
            D3D12_STATIC_SAMPLER_DESC samplerDesc = {};

            samplerDesc.MipLODBias = 0.0f;
            samplerDesc.MinLOD = 0.0f;
            samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
            samplerDesc.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY;
            samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
            samplerDesc.RegisterSpace = 0;
            samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            return samplerDesc;
        };

    //! 全てデフォルト値で初期化
    for (int i = 0; i < static_cast<int>(SamplerState::MAX); i++)
    {
        m_samplerState[i] = makeSamplerDesc();
    }

    //! POINT_WRAP
    {
        auto& desc = m_samplerState[static_cast<int>(SamplerState::POINT_WRAP)];
        desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.ShaderRegister = 0; //!< s0
    }

    //! POINT_CLAMP
    {
        auto& desc = m_samplerState[static_cast<int>(SamplerState::POINT_CLAMP)];
        desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.ShaderRegister = 1; //!< s1
    }

    //! LINEAR_WRAP
    {
        auto& desc = m_samplerState[static_cast<int>(SamplerState::LINEAR_WRAP)];
        desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.ShaderRegister = 2; //!< s2
    }

    //! LINEAR_CLAMP
    {
        auto& desc = m_samplerState[static_cast<int>(SamplerState::LINEAR_CLAMP)];
        desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.ShaderRegister = 3; //!< s3
    }

    //! ANISOTROPIC_WRAP
    {
        auto& desc = m_samplerState[static_cast<int>(SamplerState::ANISOTROPIC_WRAP)];
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.Filter = D3D12_FILTER_ANISOTROPIC;
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.ShaderRegister = 4; //!< s4
    }

    //! ANISOTROPIC_CLAMP
    {
        auto& desc = m_samplerState[static_cast<int>(SamplerState::ANISOTROPIC_CLAMP)];
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.Filter = D3D12_FILTER_ANISOTROPIC;
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.ShaderRegister = 5; //!< s5
    }
}

void PiplineState::initBlendState()
{
    //! ブレンドステートの基本設定を作成するラムダ
    auto makeBlendDesc = [&]()
        {
            D3D12_BLEND_DESC desc = {};
            desc.AlphaToCoverageEnable = FALSE;
            desc.IndependentBlendEnable = FALSE;

            auto& rt = desc.RenderTarget[0];
            rt.BlendEnable = FALSE;
            rt.LogicOpEnable = FALSE;
            rt.SrcBlend = D3D12_BLEND_ONE;
            rt.DestBlend = D3D12_BLEND_ZERO;
            rt.BlendOp = D3D12_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D12_BLEND_ONE;
            rt.DestBlendAlpha = D3D12_BLEND_ZERO;
            rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            rt.LogicOp = D3D12_LOGIC_OP_NOOP;
            rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            return desc;
        };

    //! 全てデフォルト値で初期化
    for (int i = 0; i < static_cast<int>(BlendState::MAX); i++)
    {
        m_blendState[i] = makeBlendDesc();
    }

    //! ALPHA
    {
        auto& rt = m_blendState[static_cast<int>(BlendState::ALPHA)].RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_ONE;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    }

    //! ADD
    {
        auto& rt = m_blendState[static_cast<int>(BlendState::ADD)].RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_ONE;
        rt.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlendAlpha = D3D12_BLEND_ONE;
    }

    //! MULTIPLY
    {
        auto& rt = m_blendState[static_cast<int>(BlendState::MULTIPLIE)].RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_ZERO;
        rt.DestBlend = D3D12_BLEND_SRC_COLOR;
        rt.SrcBlendAlpha = D3D12_BLEND_ZERO;
        rt.DestBlendAlpha = D3D12_BLEND_ONE;
    }
}

void PiplineState::initDepthStencilState()
{
    //! デプスステンシルステートの基本設定を作成するラムダ
    auto makeDepthStencilDesc = [&]()
        {
            D3D12_DEPTH_STENCIL_DESC desc = {};
            desc.StencilEnable = FALSE;
            desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
            desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
            desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
            desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
            desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
            desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
            desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

            return desc;
        };

    //! 全てデフォルト値で初期化
    for (int i = 0; i < static_cast<int>(DepthStencilState::MAX); i++)
    {
        m_depthStencilState[i] = makeDepthStencilDesc();
    }

    //! DEPTH_NONE
    {
        auto& desc = m_depthStencilState[static_cast<int>(DepthStencilState::DEPTH_NONE)];
        desc.DepthEnable = FALSE;
        desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    //! DEPTH_DEFALT
    {
        auto& desc = m_depthStencilState[static_cast<int>(DepthStencilState::DEPTH_DEFALT)];
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    //! DEPTH_READ
    {
        auto& desc = m_depthStencilState[static_cast<int>(DepthStencilState::DEPTH_READ)];
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    //! DEPTH_REVERSE_Z
    {
        auto& desc = m_depthStencilState[static_cast<int>(DepthStencilState::DEPTH_REVERSE_Z)];
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    }

    //! DEPTH_READ_REVERSE_Z
    {
        auto& desc = m_depthStencilState[static_cast<int>(DepthStencilState::DEPTH_READ_REVERSE_Z)];
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    }
}

void PiplineState::initRasterizerState()
{
    //! ラスタライザーステートの基本設定を作成するラムダ
    auto makeRasterizerDesc = [&]()
        {
            D3D12_RASTERIZER_DESC desc = {};

            desc.FrontCounterClockwise = FALSE;
            desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            desc.DepthClipEnable = TRUE;
            desc.MultisampleEnable = TRUE;
            desc.AntialiasedLineEnable = FALSE;
            desc.ForcedSampleCount = 0;
            desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

            return desc;
        };

    //! 全てデフォルト値で初期化
    for (int i = 0; i < static_cast<int>(RasterizerState::MAX); i++)
    {
        m_rasterizerState[i] = makeRasterizerDesc();
    }

    //! CULL_NONE
    {
        auto& desc = m_rasterizerState[static_cast<int>(RasterizerState::CULL_NONE)];
        desc.FillMode = D3D12_FILL_MODE_SOLID;
        desc.CullMode = D3D12_CULL_MODE_NONE;
    }

    //! CULL_CLOCKWISE
    {
        auto& desc = m_rasterizerState[static_cast<int>(RasterizerState::CULL_CLOCKWISE)];
        desc.FillMode = D3D12_FILL_MODE_SOLID;
        desc.CullMode = D3D12_CULL_MODE_FRONT;
    }

    //! CULL_COUNTER_CLOCKWISE
    {
        auto& desc = m_rasterizerState[static_cast<int>(RasterizerState::CULL_COUNTER_CLOCKWISE)];
        desc.FillMode = D3D12_FILL_MODE_SOLID;
        desc.CullMode = D3D12_CULL_MODE_BACK;
    }

    //! WIRE_FRAME
    {
        auto& desc = m_rasterizerState[static_cast<int>(RasterizerState::WIRE_FRAME)];
        desc.FillMode = D3D12_FILL_MODE_WIREFRAME;
        desc.CullMode = D3D12_CULL_MODE_NONE;
    }
}