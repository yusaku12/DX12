#pragma once

//! RootSignature の種類
enum class RootSignatureType
{
    Standard,
    Max
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

    //! 初期化
    void initialize();

    //! 指定した名前の RootSignature を取得
    ID3D12RootSignature* getRootSignature(RootSignatureType type) const;

private:

    RootSignatureManager() = default;

    //! Starndardをビルド
    void buildStandard();

    RootSignatureType m_buildingType = RootSignatureType::Max;
    std::array<Microsoft::WRL::ComPtr<ID3D12RootSignature>, static_cast<size_t>(RootSignatureType::Max)> m_rootSignatures;
};