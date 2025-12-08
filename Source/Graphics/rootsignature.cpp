#include "pch.h"
#include "rootsignature.h"

void RootSignature::addParameter(const D3D12_ROOT_PARAMETER& param)
{
    m_parameters.push_back(param);
}

void RootSignature::addStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler)
{
    m_samplers.push_back(sampler);
}

Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature::build() const
{
    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        static_cast<UINT>(m_parameters.size()),
        m_parameters.data(),
        static_cast<UINT>(m_samplers.size()),
        m_samplers.data(),
        m_flags
    );

    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &sigBlob,
        &errBlob);

    if (FAILED(hr))
    {
        if (errBlob)
            OutputDebugStringA((char*)errBlob->GetBufferPointer());

        throw std::runtime_error("Failed to serialize RootSignature");
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> signature;
    hr = DX12::Instance().getDevice()->CreateRootSignature(
        0,
        sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&signature));

    if (FAILED(hr))
        throw std::runtime_error("Failed to create RootSignature");

    return signature;
}