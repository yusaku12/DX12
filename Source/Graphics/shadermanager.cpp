#include "pch.h"
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

void ShaderManager::initialize()
{
    for (int i = 0; i < static_cast<int>(ShaderID::MAX); ++i)
    {
        loadShader(static_cast<ShaderID>(i));
    }

    const std::filesystem::path includePath = L"HLSL/MaterialGraphGenerated.hlsli";
    std::error_code ec;
    if (std::filesystem::exists(includePath, ec) && !ec)
    {
        m_materialGraphIncludeWriteTime = std::filesystem::last_write_time(includePath, ec);
        m_hasMaterialGraphIncludeTime = !ec;
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

    refreshMaterialGraphDependentShaders();

    // シェーダーの更新をPSOCreatorに通知
    PSOCreator::Instance().refreshDirtyPSOs();
}

void ShaderManager::refreshMaterialGraphDependentShaders()
{
    const std::filesystem::path includePath = L"HLSL/MaterialGraphGenerated.hlsli";
    std::error_code ec;
    if (!std::filesystem::exists(includePath, ec) || ec)
    {
        return;
    }

    const auto currentWriteTime = std::filesystem::last_write_time(includePath, ec);
    if (ec)
    {
        return;
    }

    if (m_hasMaterialGraphIncludeTime && currentWriteTime == m_materialGraphIncludeWriteTime)
    {
        return;
    }

    m_materialGraphIncludeWriteTime = currentWriteTime;
    m_hasMaterialGraphIncludeTime = true;

    LOG_INFO("MaterialGraph include updated. Reloading dependent shaders.");

    const ShaderID deps[] =
    {
        ShaderID::FBXPS,
        ShaderID::GBufferPS,
        ShaderID::GpuEffectPS,
        ShaderID::ColorGradingPS,
    };

    for (ShaderID dep : deps)
    {
        if (!reloadShader(dep))
        {
            LOG_ERROR("Failed to reload shader dependent on MaterialGraphGenerated.hlsli");
        }
    }
}

void ShaderManager::loadShader(ShaderID id)
{
    auto& shader = m_shaders[static_cast<size_t>(id)];
    shader.desc = shaderTable[static_cast<size_t>(id)];

    const std::wstring csoPath = getCsoPath(shader.desc);
    const std::wstring hlslPath = getHlslPath(shader.desc);

    const bool hasHlsl = std::filesystem::exists(hlslPath);
    const bool hasCso = std::filesystem::exists(csoPath);

    // CSO が存在すれば読み込む
    if (hasCso)
    {
        if (SUCCEEDED(D3DReadFileToBlob(csoPath.c_str(), shader.blob.ReleaseAndGetAddressOf())))
        {
            LOG_INFO(("Loaded CSO: " + wstringToString(csoPath) + " (" + std::to_string(shader.blob->GetBufferSize()) + " bytes)").c_str());
        }
    }

    bool shouldCompileFromHlsl = !shader.blob;

    // HLSL が存在し、CSO が無い or HLSL の方が新しいなら再コンパイル
    if (hasHlsl)
    {
        if (!hasCso)
        {
            shouldCompileFromHlsl = true;
        }
        else
        {
            const auto hlslWriteTime = std::filesystem::last_write_time(hlslPath);
            const auto csoWriteTime = std::filesystem::last_write_time(csoPath);
            if (hlslWriteTime > csoWriteTime)
            {
                shouldCompileFromHlsl = true;
            }
        }
    }

    if (shouldCompileFromHlsl)
    {
        reloadShader(id);
    }

    if (hasHlsl)
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