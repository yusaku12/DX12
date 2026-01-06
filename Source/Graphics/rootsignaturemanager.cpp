#include "pch.h"

void RootSignatureManager::begin(const std::string& name)
{
    m_buildingName = name;
    m_parameters.clear();
    m_samplers.clear();
    m_flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

void RootSignatureManager::addParameter(const D3D12_ROOT_PARAMETER& param)
{
    m_parameters.push_back(param);
}

void RootSignatureManager::addStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler)
{
    m_samplers.push_back(sampler);
}

void RootSignatureManager::build()
{
    assert(!m_buildingName.empty() && "RootSignature name is empty!");

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        static_cast<UINT>(m_parameters.size()),
        m_parameters.data(),
        static_cast<UINT>(m_samplers.size()),
        m_samplers.data(),
        m_flags
    );

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &blob,
        &error
    );

    if (FAILED(hr))
    {
        if (error)
        {
            OutputDebugStringA(
                static_cast<const char*>(error->GetBufferPointer()));
        }
        assert(false && "Failed to serialize RootSignature");
        return;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
    hr = DX12::Instance().getDevice()->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(&rootSig)
    );

    if (FAILED(hr))
    {
        assert(false && "Failed to create RootSignature");
        return;
    }

    //! 登録（同名は上書き）
    m_rootSignatures[m_buildingName] = rootSig;

    //! リセット
    m_buildingName.clear();
    m_parameters.clear();
    m_samplers.clear();
}

ID3D12RootSignature* RootSignatureManager::getRootSignature(const std::string& name) const
{
    auto it = m_rootSignatures.find(name);
    if (it == m_rootSignatures.end())
    {
        assert(false && "RootSignature not found!");
        return nullptr;
    }
    return it->second.Get();
}

bool RootSignatureManager::exists(const std::string& name) const
{
    return m_rootSignatures.find(name) != m_rootSignatures.end();
}

void RootSignatureManager::clear()
{
    m_rootSignatures.clear();
}