#pragma once

//! シェーダを読み込む際の判別
enum class ShaderType : int
{
    VS,
    PS
};

//! ShaderKey: path + type をハッシュ化してキャッシュキーにする
struct ShaderKey
{
    std::wstring path = {};
    ShaderType type = {};

    bool operator==(const ShaderKey& o) const noexcept
    {
        return path == o.path && type == o.type;
    }
};

struct ShaderKeyHash
{
    std::size_t operator()(ShaderKey const& k) const noexcept
    {
        std::size_t h1 = std::hash<std::wstring>()(k.path);
        std::size_t h2 = std::hash<int>()(static_cast<int>(k.type));
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

//=====================================================
// LoadShader クラス
//=====================================================
class LoadShader
{
public:

    LoadShader(const std::wstring& filePath, ShaderType shaderType, D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpipeline);

    //! エラーメッセージを取得
    std::string getErrorString() const;

    //! result取得
    HRESULT getResult() const { return m_result; }

private:

    //! シェーダーをセットする
    void setShader(D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpipeline);

    //! シェーダ読み込み
    HRESULT loadShader();

    std::wstring m_filePath = {};
    ShaderType m_shaderType = {};
    HRESULT m_result;
    Microsoft::WRL::ComPtr<ID3DBlob> m_shaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> m_errorBlob;
};