#pragma once

//! RootSignature の種類
enum class RootSignatureType
{
    Standard,
    Count
};

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
    void begin(RootSignatureType type);

    //!　パラメータ追加
    void addParameter(const D3D12_ROOT_PARAMETER& param);

    //! サンプラーステート追加
    void addStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler);

    //! RootSignature 作成
    void build();

    //!　指定した名前で RootSignature 作成開始(初期化時だけ有効)
    void addParameterTo(RootSignatureType type, const D3D12_ROOT_PARAMETER& param);

    //! 指定した名前の RootSignature を取得
    ID3D12RootSignature* getRootSignature(RootSignatureType type) const;

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
    void rebuild(RootSignatureType type);

    RootSignatureType m_buildingType = RootSignatureType::Count;

    std::array<RootSignatureDefinition, static_cast<size_t>(RootSignatureType::Count)> m_definitions;
    std::array<Microsoft::WRL::ComPtr<ID3D12RootSignature>, static_cast<size_t>(RootSignatureType::Count)> m_rootSignatures;
};