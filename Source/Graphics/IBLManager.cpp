#include "pch.h"
#include "Graphics/IBLManager.h"

void IBLManager::initialize()
{
    m_descriptorBase = UINT_MAX;
    m_irradiance = nullptr;
    m_prefilter = nullptr;
    m_irradiancePath.clear();
    m_prefilterPath.clear();
}

void IBLManager::setEnvironmentCubemap(const std::wstring& path)
{
    m_irradiancePath = buildIblPath(path, L"_irradiance.dds");
    m_prefilterPath = buildIblPath(path, L"_prefilter.dds");

    m_irradiance = TextureManager::Instance().load(m_irradiancePath);
    m_prefilter = TextureManager::Instance().load(m_prefilterPath);

    rebuildDescriptorTable();
}

void IBLManager::setIrradianceCubemap(const std::wstring& path)
{
    m_irradiancePath = path;
    m_irradiance = TextureManager::Instance().load(path);
    rebuildDescriptorTable();
}

void IBLManager::setPrefilteredCubemap(const std::wstring& path)
{
    m_prefilterPath = path;
    m_prefilter = TextureManager::Instance().load(path);
    rebuildDescriptorTable();
}

std::wstring IBLManager::buildIblPath(const std::wstring& basePath, const wchar_t* suffix) const
{
    std::filesystem::path p(basePath);
    p.replace_extension();
    std::wstring stem = p.wstring();
    return stem + suffix;
}

void IBLManager::rebuildDescriptorTable()
{
    LoadTexture* irradiance = m_irradiance ? m_irradiance : m_prefilter;
    LoadTexture* prefilter = m_prefilter ? m_prefilter : m_irradiance;

    if (!irradiance || !prefilter)
    {
        LOG_WARN("[IBLManager] IBL テクスチャが未設定です");
        return;
    }

    if (m_descriptorBase == UINT_MAX)
    {
        m_descriptorBase = DescriptorHeapManager::Instance().allocateRange(2);
        if (m_descriptorBase == UINT_MAX)
        {
            LOG_WARN("[IBLManager] Descriptor 確保に失敗しました");
            return;
        }
    }

    std::vector<UINT> srvIndices =
    {
        irradiance->getSRVIndex(),
        prefilter->getSRVIndex()
    };

    DescriptorHeapManager::Instance().copyDescriptorsRange(m_descriptorBase, srvIndices);
}

D3D12_GPU_DESCRIPTOR_HANDLE IBLManager::getDescriptorHandle() const
{
    if (m_descriptorBase == UINT_MAX) return {};
    return DescriptorHeapManager::Instance().getGPUHandle(m_descriptorBase);
}