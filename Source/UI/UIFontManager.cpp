#include "pch.h"
#include "UIFontManager.h"

void UIFontManager::initialize(const std::wstring& fontFace, int pixelHeight)
{
    if (m_initialized) return;

    m_lineHeight = static_cast<float>(pixelHeight);

    // 笏笏 GDI 繧ｳ繝ｳ繝・く繧ｹ繝域ｺ門ｙ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    HDC hdc = CreateCompatibleDC(nullptr);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkColor(hdc, RGB(0, 0, 0));

    HFONT hFont = CreateFontW(
        -pixelHeight,           // 譁・ｭ励そ繝ｫ縺ｮ鬮倥＆・郁ｲ蛟､ = 繝斐け繧ｻ繝ｫ謖・ｮ夲ｼ・
        0,                      // 蟷ｳ蝮・枚蟄怜ｹ・ｼ・ = 閾ｪ蜍包ｼ・
        0, 0,                   // 蛯ｾ縺阪・蛯ｾ縺崎ｧ貞ｺｦ
        FW_NORMAL,              // 螟ｪ縺・
        FALSE, FALSE, FALSE,    // italic, underline, strikeout
        DEFAULT_CHARSET,
        OUT_TT_PRECIS,          // TrueType 蜆ｪ蜈・
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,    // 繧｢繝ｳ繝√お繧､繝ｪ繧｢繧ｹ
        DEFAULT_PITCH | FF_DONTCARE,
        fontFace.c_str()
    );

    if (!hFont)
    {
        // 繝輔か繝ｼ繝ｫ繝舌ャ繧ｯ: Arial
        hFont = CreateFontW(-pixelHeight, 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    }

    SelectObject(hdc, hFont);

    // 笏笏 繧｢繝医Λ繧ｹ繝舌ャ繝輔ぃ・・8_UNORM・俄楳笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    std::vector<uint8_t> atlasData(k_atlasWidth * k_atlasHeight, 0);

    int penX = 1;
    int penY = 1;
    int rowH = 0;

    // ASCII 32縲・26・医せ繝壹・繧ｹ縲懊メ繝ｫ繝・・
    for (uint32_t cp = 32; cp < 127; ++cp)
    {
        bakeGlyph(hdc, static_cast<wchar_t>(cp),
                  atlasData, k_atlasWidth, k_atlasHeight,
                  penX, penY, rowH);
    }

    DeleteObject(hFont);
    DeleteDC(hdc);

    // 笏笏 DX12 繝・け繧ｹ繝√Ε縺ｸ繧｢繝・・繝ｭ繝ｼ繝・笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    m_atlasTexture = DXMem::makeUnique<LoadTexture>(
        k_atlasWidth, k_atlasHeight,
        DXGI_FORMAT_R8_UNORM,
        atlasData.data(),
        atlasData.size());

    m_initialized = true;
    LOG_INFO("UIFontManager: font atlas baked ({} x {})", k_atlasWidth, k_atlasHeight);
}

void UIFontManager::shutdown()
{
    m_atlasTexture.reset();
    m_glyphs.clear();
    m_initialized = false;
}

bool UIFontManager::bakeGlyph(HDC hdc,
                              wchar_t ch,
                              std::vector<uint8_t>& atlasData,
                              int atlasW, int atlasH,
                              int& penX, int& penY, int& rowH)
{
    GLYPHMETRICS gm{};
    MAT2 mat2 = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };

    //! 繝舌ャ繝輔ぃ繧ｵ繧､繧ｺ繧貞叙蠕暦ｼ・GO_GRAY8_BITMAP = 64 谿ｵ髫弱げ繝ｬ繝ｼ繧ｹ繧ｱ繝ｼ繝ｫ・・
    const DWORD bufSize = GetGlyphOutlineW(
        hdc, static_cast<UINT>(ch),
        GGO_GRAY8_BITMAP, &gm, 0, nullptr, &mat2);

    //! 繧ｹ繝壹・繧ｹ縺ｪ縺ｩ遨ｺ繧ｰ繝ｪ繝輔・ GLYPHMETRICS 縺ｮ advance 縺縺醍匳骭ｲ
    const int glyphW   = static_cast<int>((gm.gmBlackBoxX + 3) & ~3u); // 4 繝舌う繝医い繝ｩ繧､繝ｳ
    const int glyphH   = static_cast<int>(gm.gmBlackBoxY);
    const bool hasPixel = (bufSize != GDI_ERROR && bufSize > 0 && glyphH > 0);

    if (hasPixel)
    {
        // 陦後∪縺溘℃繝√ぉ繝・け
        if (penX + glyphW + 1 > atlasW)
        {
            penX  = 1;
            penY += rowH + 2;
            rowH  = 0;
        }

        if (penY + glyphH + 1 > atlasH)
        {
            LOG_WARN("UIFontManager: atlas full, skipping codepoint {}", (int)ch);
            return false;
        }

        std::vector<uint8_t> glyphBuf(bufSize, 0);
        GetGlyphOutlineW(hdc, static_cast<UINT>(ch),
                         GGO_GRAY8_BITMAP, &gm,
                         bufSize, glyphBuf.data(), &mat2);

        // GGO_GRAY8 縺ｮ蛟､縺ｯ 0縲・4 竊・0縲・55 縺ｫ繧ｹ繧ｱ繝ｼ繝ｫ
        for (int row = 0; row < glyphH; ++row)
        {
            for (int col = 0; col < static_cast<int>(gm.gmBlackBoxX); ++col)
            {
                const uint8_t v = glyphBuf[static_cast<size_t>(row * glyphW + col)];
                atlasData[static_cast<size_t>((penY + row) * atlasW + penX + col)]
                    = static_cast<uint8_t>((static_cast<uint32_t>(v) * 255u) / 64u);
            }
        }

        UIGlyphInfo info{};
        info.uv0      = Vector2(static_cast<float>(penX)                  / atlasW,
                                static_cast<float>(penY)                  / atlasH);
        info.uv1      = Vector2(static_cast<float>(penX + gm.gmBlackBoxX) / atlasW,
                                static_cast<float>(penY + glyphH)         / atlasH);
        info.advance  = static_cast<float>(gm.gmCellIncX);
        info.bearingX = static_cast<float>(gm.gmptGlyphOrigin.x);
        info.bearingY = static_cast<float>(gm.gmptGlyphOrigin.y);
        info.width    = static_cast<float>(gm.gmBlackBoxX);
        info.height   = static_cast<float>(glyphH);
        m_glyphs[static_cast<uint32_t>(ch)] = info;

        penX += glyphW + 2;
        rowH  = std::max(rowH, glyphH);
    }
    else
    {
        //! 遨ｺ繧ｰ繝ｪ繝包ｼ医せ繝壹・繧ｹ遲会ｼ・ advance 縺ｮ縺ｿ逋ｻ骭ｲ
        UIGlyphInfo info{};
        info.uv0     = Vector2(0.f, 0.f);
        info.uv1     = Vector2(0.f, 0.f);
        info.advance = static_cast<float>(gm.gmCellIncX);
        info.width   = 0.f;
        info.height  = 0.f;
        m_glyphs[static_cast<uint32_t>(ch)] = info;
    }

    return true;
}

const UIGlyphInfo* UIFontManager::getGlyph(uint32_t codepoint) const
{
    auto it = m_glyphs.find(codepoint);
    return (it != m_glyphs.end()) ? &it->second : nullptr;
}

Vector2 UIFontManager::measureText(const std::string& text, float scale) const
{
    float totalW = 0.f;
    float maxH   = 0.f;

    for (unsigned char ch : text)
    {
        const UIGlyphInfo* g = getGlyph(static_cast<uint32_t>(ch));
        if (!g) { totalW += 8.f * scale; continue; }
        totalW += g->advance * scale;
        maxH    = std::max(maxH, g->height * scale);
    }

    return Vector2(totalW, maxH > 0.f ? maxH : m_lineHeight * scale);
}

UINT UIFontManager::getAtlasSrvIndex() const
{
    return m_atlasTexture ? m_atlasTexture->getSRVIndex() : UINT_MAX;
}
