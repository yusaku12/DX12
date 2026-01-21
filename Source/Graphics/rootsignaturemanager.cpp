#include "pch.h"

void RootSignatureManager::begin(RootSignatureType type)
{
    auto& def = m_definitions[static_cast<size_t>(type)];
    def.parameters.clear();
    def.samplers.clear();
    def.flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    m_buildingType = type;
}

void RootSignatureManager::addParameter(const D3D12_ROOT_PARAMETER& param)
{
    assert(m_buildingType != RootSignatureType::Count);
    m_definitions[static_cast<size_t>(m_buildingType)].parameters.push_back(param);
}

void RootSignatureManager::addStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler)
{
    assert(m_buildingType != RootSignatureType::Count);
    m_definitions[static_cast<size_t>(m_buildingType)].samplers.push_back(sampler);
}

void RootSignatureManager::build()
{
    assert(m_buildingType != RootSignatureType::Count);
    rebuild(m_buildingType);
    m_buildingType = RootSignatureType::Count;
}

void RootSignatureManager::addParameterTo(RootSignatureType type, const D3D12_ROOT_PARAMETER& param)
{
    auto& def = m_definitions[static_cast<size_t>(type)];
    def.parameters.push_back(param);
    rebuild(type);
}

ID3D12RootSignature* RootSignatureManager::getRootSignature(RootSignatureType type) const
{
    return m_rootSignatures[static_cast<size_t>(type)].Get();
}

void RootSignatureManager::clear()
{
    for (auto& rs : m_rootSignatures)
    {
        rs.Reset();
    }

    for (auto& def : m_definitions)
    {
        def.parameters.clear();
        def.samplers.clear();
    }
}

void RootSignatureManager::rebuild(RootSignatureType type)
{
    const auto& def = m_definitions[static_cast<size_t>(type)];

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        static_cast<UINT>(def.parameters.size()),
        def.parameters.data(),
        static_cast<UINT>(def.samplers.size()),
        def.samplers.data(),
        def.flags
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
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        }
        assert(false);
        return;
    }

    hr = DX12::Instance().getDevice()->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignatures[static_cast<size_t>(type)])
    );

    assert(SUCCEEDED(hr));
}