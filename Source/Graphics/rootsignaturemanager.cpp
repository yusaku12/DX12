#include "pch.h"

void RootSignatureManager::begin(const std::string& name)
{
    m_buildingName = name;

    auto& def = m_definitions[name];
    def.parameters.clear();
    def.samplers.clear();
    def.flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

void RootSignatureManager::addParameter(const D3D12_ROOT_PARAMETER& param)
{
    assert(!m_buildingName.empty());
    m_definitions[m_buildingName].parameters.push_back(param);
}

void RootSignatureManager::addStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler)
{
    assert(!m_buildingName.empty());
    m_definitions[m_buildingName].samplers.push_back(sampler);
}

void RootSignatureManager::build()
{
    assert(!m_buildingName.empty());
    rebuild(m_buildingName);
    m_buildingName.clear();
}

void RootSignatureManager::addParameterTo(const std::string& name, const D3D12_ROOT_PARAMETER& param)
{
    auto& def = m_definitions[name]; //!< なければ新規
    def.parameters.push_back(param);
    rebuild(name);
}

ID3D12RootSignature* RootSignatureManager::getRootSignature(const std::string& name) const
{
    auto it = m_rootSignatures.find(name);
    assert(it != m_rootSignatures.end());
    return it->second.Get();
}

bool RootSignatureManager::exists(const std::string& name) const
{
    return m_rootSignatures.find(name) != m_rootSignatures.end();
}

void RootSignatureManager::clear()
{
    m_rootSignatures.clear();
    m_definitions.clear();
}

void RootSignatureManager::rebuild(const std::string& name)
{
    auto it = m_definitions.find(name);
    assert(it != m_definitions.end());

    const auto& def = it->second;

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

    assert(SUCCEEDED(hr));

    //! 同名は差し替え
    m_rootSignatures[name] = rootSig;
}