#include "pch.h"
#include "UI/UIFontManager.h"
#include "UIFontManager.h"

#include <msdfgen.h>
#include <msdfgen-ext.h>

namespace
{
    std::string toUtf8(const std::wstring& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
        {
            return {};
        }

        std::string out(static_cast<size_t>(size), '\0');
        const int written = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
        if (written != size)
        {
            return {};
        }

        return out;
    }

    uint8_t toByte(float value)
    {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return static_cast<uint8_t>(std::lround(clamped * 255.0f));
    }
}

void UIFontManager::initialize(const std::wstring& fontFace, int pixelHeight)
{
    if (m_initialized)
    {
        return;
    }

    m_lineHeight = static_cast<float>(std::max(8, pixelHeight));
    m_baseFontPixels = m_lineHeight;

    std::filesystem::path fontPath;
    if (!resolveFontPath(fontFace, fontPath))
    {
        LOG_ERROR("UIFontManager: failed to resolve font path (face=%s)", toUtf8(fontFace).c_str());
        return;
    }

    msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
    if (!ft)
    {
        LOG_ERROR("UIFontManager: initializeFreetype failed");
        return;
    }

    const std::string fontPathUtf8 = fontPath.string();
    msdfgen::FontHandle* font = msdfgen::loadFont(ft, fontPathUtf8.c_str());
    if (!font)
    {
        msdfgen::deinitializeFreetype(ft);
        LOG_ERROR("UIFontManager: loadFont failed (%s)", fontPathUtf8.c_str());
        return;
    }

    msdfgen::FontMetrics metrics{};
    if (!msdfgen::getFontMetrics(metrics, font, msdfgen::FONT_SCALING_EM_NORMALIZED))
    {
        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
        LOG_ERROR("UIFontManager: getFontMetrics failed (%s)", fontPathUtf8.c_str());
        return;
    }

    m_lineHeight = static_cast<float>(std::max(1.0, metrics.lineHeight * m_baseFontPixels));

    std::vector<uint8_t> atlasData(static_cast<size_t>(k_atlasWidth) * k_atlasHeight * 4, 0);

    int penX = 1;
    int penY = 1;
    int rowH = 0;

    for (uint32_t cp = k_firstCodepoint; cp < k_lastCodepointExclusive; ++cp)
    {
        msdfgen::Shape shape;
        double advanceEm = 0.0;
        if (!msdfgen::loadGlyph(shape, font, cp, msdfgen::FONT_SCALING_EM_NORMALIZED, &advanceEm))
        {
            UIGlyphInfo fallback{};
            fallback.advance = static_cast<float>(advanceEm * m_baseFontPixels);
            m_glyphs[cp] = fallback;
            continue;
        }

        UIGlyphInfo info{};
        info.advance = static_cast<float>(advanceEm * m_baseFontPixels);

        if (shape.contours.empty())
        {
            m_glyphs[cp] = info;
            continue;
        }

        shape.normalize();
        msdfgen::edgeColoringSimple(shape, 3.0, cp * 977u);

        const msdfgen::Shape::Bounds bounds = shape.getBounds();
        const double boundsW = bounds.r - bounds.l;
        const double boundsH = bounds.t - bounds.b;
        if (boundsW <= 0.0 || boundsH <= 0.0)
        {
            m_glyphs[cp] = info;
            continue;
        }

        const double padding = 2.0;
        const double targetSize = static_cast<double>(k_msdfGlyphBitmapSize) - 2.0 * padding;
        const double scale = std::min(targetSize / boundsW, targetSize / boundsH);
        const msdfgen::Vector2 translate(padding - bounds.l * scale, padding - bounds.b * scale);
        const msdfgen::SDFTransformation transform(msdfgen::Projection(scale, translate), msdfgen::Range(k_msdfPixelRange));

        msdfgen::Bitmap<float, 3> msdf(k_msdfGlyphBitmapSize, k_msdfGlyphBitmapSize);
        msdfgen::generateMSDF(msdf, shape, transform);

        if (penX + k_msdfGlyphBitmapSize + 1 > k_atlasWidth)
        {
            penX = 1;
            penY += rowH + 2;
            rowH = 0;
        }

        if (penY + k_msdfGlyphBitmapSize + 1 > k_atlasHeight)
        {
            LOG_WARN("UIFontManager: atlas full, skipping codepoint %u", cp);
            break;
        }

        for (int y = 0; y < k_msdfGlyphBitmapSize; ++y)
        {
            for (int x = 0; x < k_msdfGlyphBitmapSize; ++x)
            {
                const float* value = msdf(x, y);
                const size_t atlasIndex = (static_cast<size_t>(penY + y) * k_atlasWidth + (penX + x)) * 4;
                atlasData[atlasIndex + 0] = toByte(value[0]);
                atlasData[atlasIndex + 1] = toByte(value[1]);
                atlasData[atlasIndex + 2] = toByte(value[2]);
                atlasData[atlasIndex + 3] = 255;
            }
        }

        info.uv0 = Vector2(static_cast<float>(penX) / k_atlasWidth, static_cast<float>(penY) / k_atlasHeight);
        info.uv1 = Vector2(static_cast<float>(penX + k_msdfGlyphBitmapSize) / k_atlasWidth,
                           static_cast<float>(penY + k_msdfGlyphBitmapSize) / k_atlasHeight);
        info.bearingX = static_cast<float>(bounds.l * m_baseFontPixels);
        info.bearingY = static_cast<float>(bounds.t * m_baseFontPixels);
        info.width = static_cast<float>(boundsW * m_baseFontPixels);
        info.height = static_cast<float>(boundsH * m_baseFontPixels);

        m_glyphs[cp] = info;

        penX += k_msdfGlyphBitmapSize + 2;
        rowH = std::max(rowH, k_msdfGlyphBitmapSize);
    }

    for (uint32_t left = k_firstCodepoint; left < k_lastCodepointExclusive; ++left)
    {
        for (uint32_t right = k_firstCodepoint; right < k_lastCodepointExclusive; ++right)
        {
            double kerningEm = 0.0;
            if (!msdfgen::getKerning(kerningEm, font, left, right, msdfgen::FONT_SCALING_EM_NORMALIZED))
            {
                continue;
            }

            if (std::abs(kerningEm) < 0.000001)
            {
                continue;
            }

            m_kerningPairs[makeKerningKey(left, right)] = static_cast<float>(kerningEm * m_baseFontPixels);
        }
    }

    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(ft);

    m_atlasTexture = DXMem::makeUnique<LoadTexture>(
        k_atlasWidth,
        k_atlasHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        atlasData.data(),
        atlasData.size());

    m_initialized = m_atlasTexture && m_atlasTexture->isValid();

    if (!m_initialized)
    {
        m_glyphs.clear();
        m_kerningPairs.clear();
        m_atlasTexture.reset();
        LOG_ERROR("UIFontManager: atlas texture creation failed");
        return;
    }

    LOG_INFO("UIFontManager: MSDF atlas baked (%d x %d) font=%s", k_atlasWidth, k_atlasHeight, fontPathUtf8.c_str());
}

void UIFontManager::shutdown()
{
    m_atlasTexture.reset();
    m_glyphs.clear();
    m_kerningPairs.clear();
    m_initialized = false;
}

const UIGlyphInfo* UIFontManager::getGlyph(uint32_t codepoint) const
{
    const auto it = m_glyphs.find(codepoint);
    return (it != m_glyphs.end()) ? &it->second : nullptr;
}

Vector2 UIFontManager::measureText(const std::string& text, float scale) const
{
    float totalW = 0.0f;
    float maxH = 0.0f;
    uint32_t prev = 0;

    for (unsigned char ch : text)
    {
        const uint32_t cp = static_cast<uint32_t>(ch);
        if (prev != 0)
        {
            totalW += lookupKerning(prev, cp) * scale;
        }

        const UIGlyphInfo* glyph = getGlyph(cp);
        if (!glyph)
        {
            totalW += 8.0f * scale;
            prev = cp;
            continue;
        }

        totalW += glyph->advance * scale;
        maxH = std::max(maxH, glyph->height * scale);
        prev = cp;
    }

    return Vector2(totalW, maxH > 0.0f ? maxH : m_lineHeight * scale);
}

UINT UIFontManager::getAtlasSrvIndex() const
{
    return m_atlasTexture ? m_atlasTexture->getSRVIndex() : UINT_MAX;
}

float UIFontManager::getKerning(uint32_t leftCodepoint, uint32_t rightCodepoint) const
{
    return lookupKerning(leftCodepoint, rightCodepoint);
}

bool UIFontManager::resolveFontPath(const std::wstring& fontFace, std::filesystem::path& outPath) const
{
    const std::filesystem::path explicitPath(fontFace);
    if (!fontFace.empty() && std::filesystem::exists(explicitPath))
    {
        outPath = explicitPath;
        return true;
    }

    const std::wstring faceLower = [&]()
    {
        std::wstring value = fontFace;
        std::transform(value.begin(), value.end(), value.begin(), ::towlower);
        return value;
    }();

    std::vector<std::filesystem::path> candidates;
    const std::filesystem::path fontsDir = L"C:/Windows/Fonts";

    if (faceLower.find(L"meiryo") != std::wstring::npos)
    {
        candidates.emplace_back(fontsDir / L"meiryo.ttc");
    }

    if (faceLower.find(L"segoe") != std::wstring::npos)
    {
        candidates.emplace_back(fontsDir / L"segoeui.ttf");
    }

    candidates.emplace_back(fontsDir / L"meiryo.ttc");
    candidates.emplace_back(fontsDir / L"segoeui.ttf");
    candidates.emplace_back(fontsDir / L"arial.ttf");

    for (const auto& candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
        {
            outPath = candidate;
            return true;
        }
    }

    return false;
}

float UIFontManager::lookupKerning(uint32_t left, uint32_t right) const
{
    const auto it = m_kerningPairs.find(makeKerningKey(left, right));
    if (it == m_kerningPairs.end())
    {
        return 0.0f;
    }

    return it->second;
}
