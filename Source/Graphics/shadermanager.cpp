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

void ShaderManager::loadShader(ShaderID id)
{
    const auto& desc = shaderTable[static_cast<size_t>(id)];
    auto& shader = m_shaders[static_cast<size_t>(id)];

    shader.desc = desc;

    std::filesystem::path shaderPath = desc.path;
    std::wstring filename = shaderPath.stem();
    std::wstring csoPath = L"Shader/" + filename + L".cso";

    if (std::filesystem::exists(csoPath))
    {
        HRESULT hrRead = D3DReadFileToBlob(csoPath.c_str(), shader.blob.ReleaseAndGetAddressOf());

        if (SUCCEEDED(hrRead))
        {
            LOG_INFO(("Loaded CSO: " + wstringToString(csoPath) + " (" + std::to_string(shader.blob->GetBufferSize()) + " bytes)").c_str());
            shader.lastWriteTime = std::filesystem::last_write_time(csoPath);
        }
    }
}

void ShaderManager::update()
{
    for (int i = 0; i < static_cast<int>(ShaderID::MAX); ++i)
    {
        auto id = static_cast<ShaderID>(i);
        auto& shader = m_shaders[i];

        std::filesystem::path hlslShaderPath = shader.desc.path;
        std::wstring hlslFileName = hlslShaderPath.stem();
        std::wstring hlslPath = L"HLSL/" + hlslFileName + L".hlsl";

        if (!std::filesystem::exists(hlslPath))
            continue;

        auto currentWriteTime = std::filesystem::last_write_time(hlslPath);

        if (currentWriteTime != shader.lastWriteTime)
        {
            LOG_INFO(("Auto Reload: " + wstringToString(hlslPath)).c_str());

            reloadShader(id);

            shader.lastWriteTime = currentWriteTime;
        }
    }
}

void ShaderManager::reloadShader(ShaderID id)
{
    auto& shader = m_shaders[static_cast<size_t>(id)];
    const auto& desc = shader.desc;

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> newBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    std::filesystem::path hlslShaderPath = shader.desc.path;
    std::wstring hlslFileName = hlslShaderPath.stem();
    std::wstring hlslPath = L"HLSL/" + hlslFileName + L".hlsl";

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
            std::string err(
                static_cast<const char*>(error->GetBufferPointer()),
                error->GetBufferSize()
            );
            LOG_ERROR(err.c_str());
            OutputDebugStringA(err.c_str());
        }
        return;
    }

    shader.blob = newBlob;
    shader.dirty = true;

    std::filesystem::path shaderPath = desc.path;
    std::wstring filename = shaderPath.stem();
    std::wstring csoPath = L"Shader/" + filename + L".cso";

    D3DWriteBlobToFile(shader.blob.Get(), csoPath.c_str(), TRUE);

    LOG_INFO(("HotReload success: " + wstringToString(desc.path)).c_str());
}