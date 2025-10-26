#pragma once

//! �u�����h�X�e�[�g
enum class BlendState
{
    OPAQUE,
    ALPHA,
    ADD,
    MULTIPLIE
};

//! �f�v�X�X�e���V���X�e�[�g
enum class DepthStencilState
{
    DEPTH_NONE,
    DEPTH_DEFALT,
    DEPTH_READ,
    DEPTH_REVERSE_Z,
    DEPTH_READ_REVERSE_Z
};

//! ���X�^���C�U�X�e�[�g
enum class RasterizerState
{
    CULL_NONE,
    CULL_CLOCKWISE,
    CULL_COUNTER_CLOCKWISE,
    WIRE_FRAME
};

//! �T���v���[�X�e�[�g
enum class SamplerState
{
    POINT_WRAP,
    POINT_CLAMP,
    LINEAR_WRAP,
    LINEAR_CLAMP,
    ANISOTROPIC_WRAP,
    ANISOTROPIC_CLAMP
};

//! �T���v���[�X�e�[�g�ݒ�
const D3D12_STATIC_SAMPLER_DESC& setSamplerState(SamplerState samplerState, D3D12_SHADER_VISIBILITY shderType, int shderSlot, int spaceSlot);

//! �u�����h�X�e�[�g�ݒ�
D3D12_BLEND_DESC setBlendState(BlendState blendState);

//! �f�v�X�X�e���V���X�e�[�g�ݒ�
D3D12_DEPTH_STENCIL_DESC setDepthStencilState(DepthStencilState depthStencilState);

//! ���X�^���C�U�X�e�[�g�ݒ�
D3D12_RASTERIZER_DESC setRasterizerState(RasterizerState rasterizerState);

//! �p�C�v���C���̐ݒ�
void setPlpelineStateObject(D3D12_GRAPHICS_PIPELINE_STATE_DESC* psoDesc, BlendState blendState, DepthStencilState depthStencilState, RasterizerState rasterizerState);
