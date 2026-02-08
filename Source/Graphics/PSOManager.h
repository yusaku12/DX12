#pragma once

//=====================================================
// パイプラインステート管理シングルトン
//=====================================================

class PSOManager
{
public:

    //! インスタンス取得
    static PSOManager& Instance()
    {
        static PSOManager instance;
        return instance;
    }

    //! PSOデータ構造体
    struct PSOData
    {
        RootSignatureType rootSignatureType = RootSignatureType::Standard;
        ShaderID vsShaderId = ShaderID::MAX;
        ShaderID psShaderId = ShaderID::MAX;
        RasterizerState rasterizerState = RasterizerState::CULL_NONE;
        BlendState blendState = BlendState::OPAQUE;
        DepthStencilState depthStencilState = DepthStencilState::DEPTH_DEFALT;
    };

    //! PSO作成
    void createPSO(const PSOData& psoData);

    //! PSO設定
    void setPSO();

private:

    //! コンストラクタ・デストラクタ
    PSOManager() = default;
    ~PSOManager() = default;
    PSOManager(const PSOManager&) = delete;
    PSOManager& operator=(const PSOManager&) = delete;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState = nullptr;
};
