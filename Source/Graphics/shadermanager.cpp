#include "pch.h"

LoadShader* ShaderManager::load(const std::wstring& filePath, ShaderType shaderType, D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpipeline)
{
    ShaderKey key{ filePath, shaderType };
    auto it = m_shaderCache.find(key);
    if (it != m_shaderCache.end())
    {
        return it->second.get();
    }

    //! 新規ロード
    auto newShader = std::make_unique<LoadShader>(filePath, shaderType, gpipeline);
    LoadShader* shaderPtr = newShader.get();

    if (FAILED(newShader->getResult()))
    {
        //! 失敗時はキャッシュに入れず nullptr を返す
        std::string err = newShader->getErrorString();
        std::wstring logmsg = L"[ShaderManager] Failed to load shader: " + filePath;
        LOG_ERROR(wstringToString(logmsg).c_str());
        if (!err.empty()) LOG_ERROR(err.c_str());
        return nullptr;
    }

    m_shaderCache.emplace(key, std::move(newShader));

    std::wstring filename = std::wstring(L"[ShaderManager] Loaded: ") + filePath;
    LOG_INFO(wstringToString(filename).c_str());
    return shaderPtr;
}

void ShaderManager::unload(const std::wstring& filePath, ShaderType shaderType)
{
    ShaderKey key{ filePath, shaderType };
    auto it = m_shaderCache.find(key);
    if (it != m_shaderCache.end())
    {
        m_shaderCache.erase(it);
        std::wstring msg = L"[ShaderManager] Unloaded: " + filePath;
        LOG_INFO(wstringToString(msg).c_str());
    }
    else
    {
        std::wstring msg = L"[ShaderManager] Unload requested but not found: " + filePath;
        LOG_WARN(wstringToString(msg).c_str());
    }
}

void ShaderManager::clear()
{
    m_shaderCache.clear();
    LOG_INFO("[ShaderManager] Cleared all shaders");
}