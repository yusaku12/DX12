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
//! FreeType + msdfgen を用いて ASCII グリフを
//! MSDF アトラスへ焼き込む。
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

    //! 2 文字間のカーニング値（ピクセル）
    float getKerning(uint32_t leftCodepoint, uint32_t rightCodepoint) const;

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

    bool resolveFontPath(const std::wstring& fontFace, std::filesystem::path& outPath) const;

    float lookupKerning(uint32_t left, uint32_t right) const;

    static uint64_t makeKerningKey(uint32_t left, uint32_t right)
    {
        return (static_cast<uint64_t>(left) << 32) | static_cast<uint64_t>(right);
    }

    static constexpr int k_atlasWidth = 1024;
    static constexpr int k_atlasHeight = 1024;
    static constexpr int k_firstCodepoint = 32;
    static constexpr int k_lastCodepointExclusive = 127;
    static constexpr int k_msdfGlyphBitmapSize = 48;
    static constexpr double k_msdfPixelRange = 4.0;

    std::unordered_map<uint32_t, UIGlyphInfo> m_glyphs;
    std::unordered_map<uint64_t, float> m_kerningPairs;
    std::unique_ptr<class LoadTexture>        m_atlasTexture;
    float m_lineHeight = 20.f;
    float m_baseFontPixels = 20.f;
    bool  m_initialized = false;
};
