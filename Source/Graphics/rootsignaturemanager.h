#pragma once

//====================================================
// RootSignature管理シングルトン
//====================================================
class RootSignatureManager
{
public:

    //! シングルトンインスタンス取得
    static RootSignatureManager& Instance()
    {
        static RootSignatureManager instance;
        return instance;
    }

    //! RootSignature作成
    void begin(const std::string& name);

    //! パラメータ追加
    void addParameter(const D3D12_ROOT_PARAMETER& param);

    //! スタティックサンプラー追加
    void addStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler);

    //! RootSignature構築
    void build();

    //! RootSignature取得
    ID3D12RootSignature* getRootSignature(const std::string& name) const;

    //! RootSignature存在確認
    bool exists(const std::string& name) const;

    //! 全RootSignature解放
    void clear();

    //! フラグ設定
    void setFlags(D3D12_ROOT_SIGNATURE_FLAGS flags) { m_flags = flags; }

private:

    RootSignatureManager() = default;
    ~RootSignatureManager() = default;
    RootSignatureManager(const RootSignatureManager&) = delete;
    RootSignatureManager& operator=(const RootSignatureManager&) = delete;

    //! 登録済みRootSignature
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> m_rootSignatures;

    //! Build中データ
    std::string m_buildingName;
    std::vector<D3D12_ROOT_PARAMETER> m_parameters;
    std::vector<D3D12_STATIC_SAMPLER_DESC> m_samplers;
    D3D12_ROOT_SIGNATURE_FLAGS m_flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
};