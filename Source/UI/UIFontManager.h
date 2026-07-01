#pragma once

//=====================================================
//! グリフ情報（フォントアトラス内の 1 文字分のメタデータ）
//=====================================================
struct UIGlyphInfo
{
    Vector2 uv0;       //!< アトラス内 UV 左上
    Vector2 uv1;       //!< アトラス内 UV 右下
    float   advance;   //!< 次の文字までの水平移動量（ピクセル）
    float   bearingX;  //!< グリフ描画オフセット X（カーソルから）
    float   bearingY;  //!< グリフ描画オフセット Y（ベースラインから上が正）
    float   width;     //!< グリフ幅（ピクセル）
    float   height;    //!< グリフ高さ（ピクセル）
};

//=====================================================
//! フォントアトラスマネージャー
//!
//! Windows GDI の GGO_GRAY8_BITMAP を用いて ASCII グリフを
//! 512x512 の R8_UNORM テクスチャアトラスに焼き込む。
//! 外部ライブラリ不要・ランタイムコンパイルのみで動作する。
//=====================================================
class UIFontManager
{
public:

    static UIFontManager& Instance()
    {
        static UIFontManager instance;
        return instance;
    }

    //! アトラスを焼き込む
    //! @param fontFace   フォントフェイス名（例: L"Arial"）
    //! @param pixelHeight  フォントの高さ（ピクセル）
    void initialize(const std::wstring& fontFace = L"Meiryo UI",
        int pixelHeight = 20);

    //! 破棄
    void shutdown();

    //! 指定コードポイントのグリフ情報を取得（nullptr = 未登録）
    const UIGlyphInfo* getGlyph(uint32_t codepoint) const;

    //! テキストの描画サイズを計算
    Vector2 measureText(const std::string& text, float scale = 1.0f) const;

    //! フォントアトラステクスチャの SRV インデックス
    UINT getAtlasSrvIndex() const;

    //! 行の高さ（ピクセル）
    float getLineHeight() const { return m_lineHeight; }

    bool isInitialized() const { return m_initialized; }

private:

    UIFontManager() = default;
    ~UIFontManager() = default;
    UIFontManager(const UIFontManager&) = delete;
    UIFontManager& operator=(const UIFontManager&) = delete;

    //! 指定文字を HDC/HFONT でラスタライズしてアトラスに書き込む
    //! @return 書き込み成功なら true
    bool bakeGlyph(HDC hdc,
        wchar_t ch,
        std::vector<uint8_t>& atlasData,
        int atlasW, int atlasH,
        int& penX, int& penY, int& rowH);

    static constexpr int k_atlasWidth = 512;
    static constexpr int k_atlasHeight = 512;

    std::unordered_map<uint32_t, UIGlyphInfo> m_glyphs;
    std::unique_ptr<class LoadTexture>        m_atlasTexture;
    float m_lineHeight = 20.f;
    bool  m_initialized = false;
};
