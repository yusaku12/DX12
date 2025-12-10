#pragma once

//=====================================================
// ルートシグネチャマネージャの構築を支援するクラス
//=====================================================
class RootSignature
{
public:

    RootSignature() = default;

    //! Root Parameter を追加
    void addParameter(const D3D12_ROOT_PARAMETER& param);

    //! Static Sampler を追加
    void addStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler);

    //! フラグを設定
    void setFlags(D3D12_ROOT_SIGNATURE_FLAGS flags) { m_flags = flags; }

    //! Root Signature を構築
    Microsoft::WRL::ComPtr<ID3D12RootSignature> build() const;

private:
    std::vector<D3D12_ROOT_PARAMETER> m_parameters;
    std::vector<D3D12_STATIC_SAMPLER_DESC> m_samplers;
    D3D12_ROOT_SIGNATURE_FLAGS m_flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
};