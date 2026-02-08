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

    std::filesystem::path shaderPath = desc.path;
    std::wstring filename = shaderPath.stem();
    std::wstring csoPath = L"Shader/" + filename + L".cso";

    // .cso 読み込み
    if (std::filesystem::exists(csoPath))
    {
        HRESULT hrRead = D3DReadFileToBlob(
            csoPath.c_str(),
            m_shaderBlobs[static_cast<size_t>(id)].GetAddressOf()
        );

        if (SUCCEEDED(hrRead))
        {
            LOG_INFO(("Loaded CSO: " + wstringToString(csoPath) +
                " (" + std::to_string(m_shaderBlobs[static_cast<size_t>(id)]->GetBufferSize()) + " bytes)").c_str());
            return;
        }
        else
        {
            std::wstring msg = L"Failed to read CSO (will recompile): " + csoPath +
                L"  HRESULT=0x" + std::to_wstring(static_cast<unsigned long>(hrRead));

            LOG_WARN(wstringToString(msg).c_str());
            OutputDebugStringW((msg + L"\n").c_str());
        }
    }

#if 0 // 別に要らない気がしてきた。
    // ホットリロード時に使う場合多分いる
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3DCompileFromFile(
        desc.path.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        desc.entry,
        desc.profile,
        compileFlags,
        0,
        m_shaderBlobs[static_cast<size_t>(id)].GetAddressOf(),
        error.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (error)
        {
            std::string err(
                static_cast<const char*>(error->GetBufferPointer()),
                error->GetBufferSize()
            );
            OutputDebugStringA(err.c_str());
            LOG_ERROR(err.c_str());
        }
        return;
    }

    //! 成功時のみ保存
    D3DWriteBlobToFile(
        m_shaderBlobs[static_cast<size_t>(id)].Get(),
        csoPath.c_str(),
        TRUE
    );

    LOG_INFO(("Saved CSO: " + wstringToString(csoPath)).c_str());
#endif
}