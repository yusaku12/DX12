#pragma once

//=====================================================
//! ポストエフェクト基底クラス
//! 各エフェクトはこのクラスを継承して実装する
//=====================================================
class PostEffectBase
{
public:

    virtual ~PostEffectBase() = default;

    //! PSO 登録（起動時に1回呼ばれる）
    virtual void initialize() = 0;

    //! フルスクリーン描画
    //! @param cmd コマンドリスト
    //! @param inputSrvIndex 入力テクスチャの SRV インデックス
    virtual void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) = 0;

    //! ImGui インスペクタ
    virtual void inspectGUI() {}

    //! エフェクト名
    virtual const char* getName() const = 0;

    //! ピクセルシェーダーID
    virtual ShaderID getPixelShaderID() const = 0;

    //! 有効/無効
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool value) { m_enabled = value; }

    //! 描画優先度（小さいほど先に適用）
    int getPriority() const { return m_priority; }
    void setPriority(int priority) { m_priority = priority; }

    //! ブレンドウェイト
    virtual void setBlendWeight(float weight) { m_blendWeight = weight; }
    float getBlendWeight() const { return m_blendWeight; }

protected:

    bool m_enabled = true;
    int m_priority = 0;
    float m_blendWeight = 1.0f; //!< ブレンドウェイト
    size_t m_psoKey = 0;        //!< PSOCreator に登録したキー

    //! PSO 登録ヘルパー（派生クラスの initialize() で呼ぶ）
    void registerPSO(ShaderID psShaderID);

    //! PSO 登録ヘルパー（RootSignatureType 指定版）
    size_t registerPSO(ShaderID psShaderID, RootSignatureType rsType);

    //! フルスクリーン三角形を描画するヘルパー
    void drawFullscreenTriangle(ID3D12GraphicsCommandList* cmd);

    //! PSO をコマンドリストにセット
    void applyPSO(ID3D12GraphicsCommandList* cmd);

    //! 指定キーの PSO をコマンドリストにセット
    void applyPSO(size_t key, ID3D12GraphicsCommandList* cmd);
};