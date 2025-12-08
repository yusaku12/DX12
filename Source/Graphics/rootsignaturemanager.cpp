#include "pch.h"

void RootSignatureManager::add(const std::string& name, Microsoft::WRL::ComPtr<ID3D12RootSignature> signature)
{
    //! 同じ名前が登録済みの場合は上書き
    m_rootSignatures[name] = signature;
}

ID3D12RootSignature* RootSignatureManager::getRootSignature(const std::string& name)
{
    auto it = m_rootSignatures.find(name);
    if (it == m_rootSignatures.end())
    {
        assert(false && "RootSignature not found!");
        return nullptr;
    }
    return it->second.Get();
}

bool RootSignatureManager::exisit(const std::string& name) const
{
    return m_rootSignatures.find(name) != m_rootSignatures.end();
}

void RootSignatureManager::clear()
{
    m_rootSignatures.clear();
}