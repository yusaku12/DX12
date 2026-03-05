#pragma once

//=====================================================
// パイプラインステート管理シングルトン
//=====================================================
class PSOCreator
{
public:

    //! PSOデータ構造体
    struct PSOData
    {
        RootSignatureType rootSignatureType = RootSignatureType::Standard;
        ShaderID vsShaderId = ShaderID::MAX;
        ShaderID psShaderId = ShaderID::MAX;
        RasterizerState rasterizerState = RasterizerState::CULL_NONE;
        BlendState blendState = BlendState::OPAQUE;
        DepthStencilState depthStencilState = DepthStencilState::DEPTH_DEFALT;
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = {};

        //! ハッシュ値計算（inputLayoutは除外：enumの組み合わせで十分識別可能）
        size_t computeHash() const
        {
            size_t h = 0;
            hashCombine(h, static_cast<size_t>(rootSignatureType));
            hashCombine(h, static_cast<size_t>(vsShaderId));
            hashCombine(h, static_cast<size_t>(psShaderId));
            hashCombine(h, static_cast<size_t>(rasterizerState));
            hashCombine(h, static_cast<size_t>(blendState));
            hashCombine(h, static_cast<size_t>(depthStencilState));
            hashCombine(h, static_cast<size_t>(topologyType));
            return h;
        }

        bool operator==(const PSOData& other) const
        {
            return rootSignatureType == other.rootSignatureType
                && vsShaderId == other.vsShaderId
                && psShaderId == other.psShaderId
                && rasterizerState == other.rasterizerState
                && blendState == other.blendState
                && depthStencilState == other.depthStencilState
                && topologyType == other.topologyType;
        }

    private:

        static void hashCombine(size_t& seed, size_t value)
        {
            seed ^= std::hash<size_t>{}(value)+0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    };

    //! PSODataのハッシュファンクタ
    struct PSODataHash
    {
        size_t operator()(const PSOData& data) const { return data.computeHash(); }
    };

    //! シングルトンインスタンス取得
    static PSOCreator& Instance()
    {
        static PSOCreator instance;
        return instance;
    }

    //! PSOを登録し、キーとなるハッシュ値を返す
    size_t registerPSO(const PSOData& data);

    //! PSO設定（RootSignature + PipelineState をコマンドリストにセット）
    void setPSO(size_t key);

    //! PSO設定（指定コマンドリストに対して）
    void setPSO(size_t key, ID3D12GraphicsCommandList* cmd);

    //! ホットリロード対応：dirtyなシェーダを含むPSOを再構築
    void refreshDirtyPSOs();

    //! 全キャッシュクリア
    void clearAll();

private:

    PSOCreator() = default;
    ~PSOCreator() = default;
    PSOCreator(const PSOCreator&) = delete;
    PSOCreator& operator=(const PSOCreator&) = delete;

    //! PSO生成の内部実装
    Microsoft::WRL::ComPtr<ID3D12PipelineState> buildPSO(const PSOData& data);

    //! キャッシュエントリ
    struct CacheEntry
    {
        PSOData data;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    };

    //! ハッシュマップキャッシュ
    std::unordered_map<size_t, CacheEntry> m_cache;
};
