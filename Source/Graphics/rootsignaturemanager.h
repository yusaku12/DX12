#pragma once

//=====================================================
// ルートシグネチャをキャッシュするシングルトン
//=====================================================
class RootSignatureManager
{
public:

    //! シングルトンインスタンス取得
    static RootSignatureManager& Instance()
    {
        static RootSignatureManager instance;
        return instance;
    }

    // ルートシグネチャ登録
    void add(const std::string& name, Microsoft::WRL::ComPtr<ID3D12RootSignature> signature);

    // 取得
    ID3D12RootSignature* getRootSignature(const std::string& name);

    // 存在チェック
    bool exisit(const std::string& name) const;

    // 全削除
    void clear();

private:

    RootSignatureManager() = default;
    ~RootSignatureManager() = default;

    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> m_rootSignatures;
};