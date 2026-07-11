#pragma once

//=====================================================
//! UI 定数バッファレイアウト（256 バイトアライメント）
//=====================================================
struct UIConstantData
{
    Matrix   transform;       //!< OrthoProj（スクリーン）or MVP（ワールド）: 64 bytes
    Matrix   localTransform;  //!< ローカルアニメーション変換: 64 bytes
    Vector4  tintColor;       //!< グローバルティント: 16 bytes
    uint32_t textureMode;     //!< 0=カラー / 1=RGBA / 2=旧フォント / 3=MSDFフォント: 4 bytes
    float    globalAlpha;     //!< フェードアルファ [0, 1]: 4 bytes
    float    pad0 = 0.f;
    float    pad1 = 0.f;
    // 合計 160 bytes → 256 byte アライメントは ConstantBuffer<T> が処理
};

//=====================================================
//! UI 頂点フォーマット（32 bytes）
//=====================================================
struct UIVertex
{
    Vector2 position;   //!< スクリーン座標（ピクセル、左上原点）or ローカル XY
    Vector2 texcoord;
    Vector4 color;      //!< RGBA [0, 1]
};

//=====================================================
//! UI 描画コマンド
//=====================================================
struct UIDrawCommand
{
    UINT startVertex = 0;
    UINT quadCount   = 0;
    UINT srvIndex    = UINT_MAX; //!< UINT_MAX = ホワイトテクスチャ（ソリッドカラー）
    UINT textureMode = 0;
    UINT cbSlot      = 0;
};

//=====================================================
//! DirectX 12 ネイティブ UI レンダラー
//!
//! 全ウィジェットの矩形を動的頂点バッファに積み上げ、
//! フレーム末尾でバッチ描画する。
//! スクリーン空間・ワールド空間の両 Canvas に対応。
//=====================================================
class UIRenderer
{
public:

    static UIRenderer& Instance()
    {
        static UIRenderer instance;
        return instance;
    }

    //! 初期化（DX12 デバイス・RootSignature 準備後に呼ぶ）
    void initialize();

    //! 破棄
    void shutdown();

    //! フレーム描画開始（SceneManager::draw() 完了後に呼ぶ）
    void begin(float screenWidth, float screenHeight);

    //! フレーム描画終了・GPU コマンドを発行する
    void end();

    // ── 描画 API ─────────────────────────────────────

    //! ソリッド矩形
    void drawRect(float x, float y, float w, float h,
                  const Vector4& color,
                  const Matrix* localTransform = nullptr,
                  float alpha = 1.0f);

    //! RGBA テクスチャ付き矩形
    void drawTexturedRect(float x, float y, float w, float h,
                          UINT srvIndex,
                          const Vector4& tintColor = Vector4(1, 1, 1, 1),
                          const Matrix* localTransform = nullptr,
                          float alpha = 1.0f);

    //! テキスト描画（UIFontManager に委譲）
    //! @return 描画後のカーソル X 座標
    float drawText(float x, float y,
                   const std::string& text,
                   const Vector4& color,
                   float scale = 1.0f,
                   const Matrix* localTransform = nullptr,
                   float alpha = 1.0f);

    //! テキストの描画サイズを計算（描画なし）
    Vector2 measureText(const std::string& text, float scale = 1.0f) const;

    //! ワールド空間での矩形描画
    //! @param worldTransform  Canvas の GameObject ワールド行列
    //! @param viewProjection  カメラの ViewProjection 行列
    void drawWorldRect(const Matrix& worldTransform,
                       const Matrix& viewProjection,
                       float w, float h,
                       const Vector4& color,
                       float alpha = 1.0f);

    // ── 状態設定 API ─────────────────────────────────

    //! グローバルアルファ（全描画に適用）
    void setGlobalAlpha(float a) { m_globalAlpha = std::clamp(a, 0.f, 1.f); }
    float getGlobalAlpha() const { return m_globalAlpha; }

    //! ホワイトテクスチャの SRV インデックス
    UINT getWhiteSrvIndex() const { return m_whiteSrvIndex; }

    //! フォントアトラスの SRV インデックス
    UINT getFontAtlasSrvIndex() const;

    bool isInitialized() const { return m_initialized; }

private:

    UIRenderer() = default;
    ~UIRenderer() = default;
    UIRenderer(const UIRenderer&) = delete;
    UIRenderer& operator=(const UIRenderer&) = delete;

    //! 矩形を動的バッファにプッシュして描画コマンドを追加する
    void pushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  const Vector4& color,
                  UINT srvIndex, UINT textureMode,
                  const Matrix& worldTransform,
                  const Matrix& localTransform,
                  float alpha);

    //! ホワイト 1x1 テクスチャを作成する（ソリッドカラー描画用）
    void createWhiteTexture();

    //! 定数バッファにデータをアップロードして CBV スロット番号を返す
    UINT uploadConstants(const UIConstantData& data);

    //! 指定スロットの GPU 仮想アドレスを返す
    D3D12_GPU_VIRTUAL_ADDRESS getCBGPUAddress(UINT slot) const;

    //! 全描画コマンドを GPU コマンドリストに記録する
    void flushCommands(ID3D12GraphicsCommandList* cmd);

    // ─── 定数 ─────────────────────────────────────────

    static constexpr UINT k_maxVertices = 65536;           //!< 16384 クワッド
    static constexpr UINT k_maxQuads    = k_maxVertices / 4;
    //! Root CBV は GPU VA で直接バインドするためデスクリプタヒープ不要。
    //! 1 フレームの最大ユニーク定数セット数（オーバーした場合は末尾スロットを上書き）
    static constexpr UINT k_maxCBSlots  = 256;

    // ─── DX12 リソース ───────────────────────────────

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW               m_vbView{};
    UIVertex*                              m_mappedVerts = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW                m_ibView{};

    //! デスクリプタヒープを消費しない CBV リングバッファ
    //! Root CBV は GPU 仮想アドレスで直接バインドするため DescriptorHeap 不要。
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cbRingBuffer;
    uint8_t*                               m_cbMapped = nullptr;
    UINT                                   m_cbStride = 0;  //!< 256-byte アライン済みサイズ
    UINT                                   m_cbSlot   = 0;

    std::unique_ptr<class LoadTexture> m_whiteTexture;
    UINT m_whiteSrvIndex = UINT_MAX;

    size_t m_psoKey = 0;

    // ─── CPU キュー ──────────────────────────────────

    std::vector<UIVertex>      m_vertices;
    std::vector<UIDrawCommand> m_drawCommands;

    // ─── 状態 ────────────────────────────────────────

    float  m_screenWidth  = 1280.f;
    float  m_screenHeight = 720.f;
    Matrix m_orthoMatrix;
    float  m_globalAlpha  = 1.0f;
    bool   m_initialized  = false;
};
