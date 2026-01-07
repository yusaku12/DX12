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

    //!　RootSignature 作成開始
    void begin(const std::string& name);

    //!　パラメータ追加
    void addParameter(const D3D12_ROOT_PARAMETER& param);

    //! サンプラーステート追加
    void addStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler);

    //! RootSignature 作成
    void build();

    //!　指定した名前で RootSignature 作成開始(初期化時だけ有効)
    void addParameterTo(const std::string& name, const D3D12_ROOT_PARAMETER& param);

    //! 指定した名前の RootSignature を取得
    ID3D12RootSignature* getRootSignature(const std::string& name) const;

    //! 指定した名前の RootSignature が存在するか
    bool exists(const std::string& name) const;

    //! 全ての RootSignature を破棄
    void clear();

private:

    RootSignatureManager() = default;

    // RootSignature の構造体
    struct RootSignatureDefinition
    {
        std::vector<D3D12_ROOT_PARAMETER> parameters;
        std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
        D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    };

    //! 指定した名前で RootSignature を再構築
    void rebuild(const std::string& name);

    std::string m_buildingName;

    std::unordered_map<std::string, RootSignatureDefinition> m_definitions;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> m_rootSignatures;
};