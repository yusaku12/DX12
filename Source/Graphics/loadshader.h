#pragma once

//! シェーダを読み込む際の判別
enum class ShaderType
{
    VS,
    PS
};

//=====================================================
// LoadShader クラス
//=====================================================
class LoadShader
{
public:

    //! シングルトン
    static LoadShader& getInstance()
    {
        static LoadShader instance;
        return instance;
    }

    //! シェーダ読み込み
    HRESULT loadShader(const wchar_t* shaderFile, ShaderType shaderType, Microsoft::WRL::ComPtr<ID3DBlob>& shaderBlob, Microsoft::WRL::ComPtr<ID3DBlob>& errorBlob);

private:

    LoadShader() = default;
    ~LoadShader() = default;

    // コピー/ムーブ禁止
    LoadShader(const LoadShader&) = delete;
    LoadShader(LoadShader&&) = delete;
    LoadShader& operator=(const LoadShader&) = delete;
    LoadShader& operator=(LoadShader&&) = delete;

    //! シェーダ読み込み補助関数
    HRESULT compileShaderFromFile(const wchar_t* filePath, const char* entryName, const char* shaderVersion, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob, Microsoft::WRL::ComPtr<ID3DBlob>& errorBlob);
};