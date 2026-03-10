#include "pch.h"
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

void ShaderManager::initialize()
{
    for (int i = 0; i < static_cast<int>(ShaderID::MAX); ++i)
    {
        loadShader(static_cast<ShaderID>(i));
    }
}

void ShaderManager::update()
{
    for (int i = 0; i < static_cast<int>(ShaderID::MAX); ++i)
    {
        auto& shader = m_shaders[i];

        const std::wstring hlslPath = getHlslPath(shader.desc);

        if (!std::filesystem::exists(hlslPath))
            continue;

        auto currentWriteTime = std::filesystem::last_write_time(hlslPath);

        if (currentWriteTime == shader.lastWriteTime)
            continue;

        LOG_INFO(("Auto Reload: " + wstringToString(hlslPath)).c_str());

        if (reloadShader(static_cast<ShaderID>(i)))
        {
            shader.lastWriteTime = currentWriteTime;
        }
    }

    // シェーダーの更新をPSOCreatorに通知
    PSOCreator::Instance().refreshDirtyPSOs();
}

void ShaderManager::loadShader(ShaderID id)
{
    auto& shader = m_shaders[static_cast<size_t>(id)];
    shader.desc = shaderTable[static_cast<size_t>(id)];

    const std::wstring csoPath = getCsoPath(shader.desc);

    if (std::filesystem::exists(csoPath))
    {
        if (SUCCEEDED(D3DReadFileToBlob(csoPath.c_str(), shader.blob.ReleaseAndGetAddressOf())))
        {
            LOG_INFO(("Loaded CSO: " + wstringToString(csoPath) + " (" + std::to_string(shader.blob->GetBufferSize()) + " bytes)").c_str());
        }
    }

    const std::wstring hlslPath = getHlslPath(shader.desc);
    if (std::filesystem::exists(hlslPath))
    {
        shader.lastWriteTime = std::filesystem::last_write_time(hlslPath);
    }
}

bool ShaderManager::reloadShader(ShaderID id)
{
    auto& shader = m_shaders[static_cast<size_t>(id)];
    const auto& desc = shader.desc;

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> newBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    const std::wstring hlslPath = getHlslPath(desc);

    HRESULT hr = D3DCompileFromFile(
        hlslPath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        desc.entry,
        desc.profile,
        compileFlags,
        0,
        newBlob.GetAddressOf(),
        error.GetAddressOf()
    );

    if (FAILED(hr))
    {
        LOG_ERROR("HotReload failed - keeping old shader");

        if (error)
        {
            std::string err(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize());
            LOG_ERROR(err.c_str());
            OutputDebugStringA(err.c_str());
        }

        return false;
    }

    shader.blob = newBlob;
    shader.dirty = true;

    const std::wstring csoPath = getCsoPath(desc);
    D3DWriteBlobToFile(shader.blob.Get(), csoPath.c_str(), TRUE);

    LOG_INFO(("HotReload success: " + wstringToString(desc.path)).c_str());

    return true;
}

std::wstring ShaderManager::getHlslPath(const ShaderDesc& desc) const
{
    std::filesystem::path p = desc.path;
    return L"HLSL/" + p.stem().wstring() + L".hlsl";
}

std::wstring ShaderManager::getCsoPath(const ShaderDesc& desc) const
{
    std::filesystem::path p = desc.path;
    return L"Shader/" + p.stem().wstring() + L".cso";
}