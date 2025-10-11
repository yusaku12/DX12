#include "pch.h"
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

HRESULT LoadShader::loadShader(const wchar_t* shaderFile, ShaderType shaderType, Microsoft::WRL::ComPtr<ID3DBlob>& shaderBlob, Microsoft::WRL::ComPtr<ID3DBlob>& errorBlob)
{
    const char* entryName = {};
    const char* shaderVersion = {};

    //! 選ばれたシェーダのタイプで適したのを選択
    switch (shaderType)
    {
    case ShaderType::VS:
        entryName = "VS";
        shaderVersion = "vs_5_0";
        break;
    case ShaderType::PS:
        entryName = "PS";
        shaderVersion = "ps_5_0";
        break;
    }

    //! シェーダをコンパイルする
    HRESULT hr = compileShaderFromFile(shaderFile, entryName, shaderVersion, shaderBlob, errorBlob);

    if (FAILED(hr))
    {
        //! wstring → string 変換
        std::wstring ws(shaderFile);
        std::string fileName(ws.begin(), ws.end());

        //! シェーダ読み込みが出来なかった場合の詳細ログ
        std::string message = "Failed to load shader file: " + fileName;
        message += "\nEntryPoint: " + std::string(entryName);
        message += "\nShaderVersion: " + std::string(shaderVersion);

        //! HLSLコンパイルエラー情報がある場合は追加
        if (errorBlob)
        {
            message += "\nError:\n";
            message += static_cast<const char*>(errorBlob->GetBufferPointer());
        }

        Logger::getInstance().logCall(LogLevel::ERROR, message.c_str());
        return hr;
    }

    return S_OK;
}

HRESULT LoadShader::compileShaderFromFile(const wchar_t* filePath, const char* entryName, const char* shaderVersion, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob, Microsoft::WRL::ComPtr<ID3DBlob>& errorBlob)
{
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(filePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryName, shaderVersion, compileFlags, 0, outBlob.GetAddressOf(), errorBlob.GetAddressOf());

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return hr;
    }

    return S_OK;
}