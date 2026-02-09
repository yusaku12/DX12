#pragma once

//=====================================================
// パイプラインステート作成クラス
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
    };

    explicit PSOCreator(const PSOData& data);
    ~PSOCreator() {}

    //! PSO作成
    void createPSO();

    //! PSO設定
    void setPSO();

private:

    PSOData m_psoData = {};
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState = nullptr;
};
