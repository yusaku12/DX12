#include "pch.h"
#include "loadshader.h"
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

LoadShader::LoadShader(const std::wstring& filePath, ShaderType shaderType, D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpipeline)
    : m_filePath(filePath), m_shaderType(shaderType), m_result(E_FAIL)
{
    m_result = loadShader();

    //! シェーダーをセット
    setShader(gpipeline);
}

void LoadShader::setShader(D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpipeline)
{
    if (!m_shaderBlob)
    {
        Logger::Instance().logCall(LogLevel::ERROR, "Shader blob is null.");
        return;
    }

    switch (m_shaderType)
    {
    case ShaderType::VS:
        gpipeline.VS.pShaderBytecode = m_shaderBlob->GetBufferPointer();
        gpipeline.VS.BytecodeLength = m_shaderBlob->GetBufferSize();
        break;
    case ShaderType::PS:
        gpipeline.PS.pShaderBytecode = m_shaderBlob->GetBufferPointer();
        gpipeline.PS.BytecodeLength = m_shaderBlob->GetBufferSize();
        break;
    }
}

std::string LoadShader::getErrorString() const
{
    if (m_errorBlob) return std::string(static_cast<const char*>(m_errorBlob->GetBufferPointer()), m_errorBlob->GetBufferSize());
    return {};
}

HRESULT LoadShader::loadShader()
{
    //! ファイル存在チェック
    if (!std::filesystem::exists(m_filePath))
    {
        std::wstring msg = L"Shader file not found: " + m_filePath;
        Logger::Instance().logCall(LogLevel::ERROR, wstringToString(msg).c_str());
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    //! ソースからコンパイル
    const char* entryName = nullptr;
    const char* shaderVersion = nullptr;
    switch (m_shaderType)
    {
    case ShaderType::VS:
        entryName = "VS";
        shaderVersion = "vs_5_0";
        break;
    case ShaderType::PS:
        entryName = "PS";
        shaderVersion = "ps_5_0";
        break;
    default:
        Logger::Instance().logCall(LogLevel::ERROR, "Unknown shader type.");
        return E_INVALIDARG;
    }

    //! .cso が存在すればそれを読み込む
    std::filesystem::path shaderPath = m_filePath;
    std::wstring filename = shaderPath.stem();
    std::wstring csoPath = L"Shader/" + filename + L".cso";
    if (std::filesystem::exists(csoPath))
    {
        HRESULT hr = D3DReadFileToBlob(csoPath.c_str(), m_shaderBlob.GetAddressOf());
        if (SUCCEEDED(hr))
        {
            Logger::Instance().logCall(
                LogLevel::INFO,
                ("Loaded cached CSO: " + wstringToString(csoPath) +
                    " (" + std::to_string(m_shaderBlob->GetBufferSize()) + " bytes)").c_str()
            );
            return S_OK;
        }
        Logger::Instance().logCall(LogLevel::WARN, "Failed to read CSO. Compiling instead...");
    }

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(m_filePath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryName, shaderVersion, compileFlags, 0, m_shaderBlob.GetAddressOf(), m_errorBlob.GetAddressOf());
    if (FAILED(hr))
    {
        //! エラーログ
        std::string err = getErrorString();
        std::wstring logmsg = L"Failed to compile shader: " + m_filePath + L"\n";
        Logger::Instance().logCall(LogLevel::ERROR, wstringToString(logmsg + std::wstring(err.begin(), err.end())).c_str());

        //! デバッグ用に OutputDebugString
        if (m_errorBlob)
        {
            OutputDebugStringA(static_cast<const char*>(m_errorBlob->GetBufferPointer()));
        }

        return hr;
    }

    //! コンパイル成功 → .cso に保存
    HRESULT hrWrite = D3DWriteBlobToFile(m_shaderBlob.Get(), csoPath.c_str(), TRUE);
    if (FAILED(hrWrite))
    {
        Logger::Instance().logCall(LogLevel::WARN,
            ("Failed to write CSO: " + wstringToString(csoPath)).c_str());
    }
    else
    {
        Logger::Instance().logCall(
            LogLevel::INFO,
            ("Saved CSO: " + wstringToString(csoPath) +
                " (" + std::to_string(m_shaderBlob->GetBufferSize()) + " bytes)").c_str()
        );
    }
    return S_OK;
}