#include "pch.h"
#include "UIImageComponent.h"
#include "UI\UIAnimator.h"

void UIImageComponent::awake()
{
    if (!m_texturePath.empty())
        setTexturePath(m_texturePath);
}

void UIImageComponent::setTexturePath(const std::wstring& path)
{
    m_texturePath = path;
    if (!path.empty())
        m_texture = TextureManager::Instance().load(path);
    else
        m_texture = nullptr;
}

UINT UIImageComponent::getSrvIndex() const
{
    return m_texture ? m_texture->getSRVIndex() : UINT_MAX;
}

void UIImageComponent::inspectGUI()
{
    // テクスチャパス（wstring → UTF-8 変換して ImGui に渡す）
    std::array<char, 512> buf{};
    if (!m_texturePath.empty())
    {
        WideCharToMultiByte(CP_UTF8, 0,
            m_texturePath.c_str(), static_cast<int>(m_texturePath.size()),
            buf.data(), static_cast<int>(buf.size() - 1),
            nullptr, nullptr);
    }
    if (ImGui::InputText("Texture Path", buf.data(), buf.size()))
    {
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, buf.data(), -1, nullptr, 0);
        std::wstring wide(static_cast<size_t>(wlen), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, buf.data(), -1, wide.data(), wlen);
        wide.resize(wide.find(L'\0'));  // null 終端を除去
        setTexturePath(wide);
    }

    float tc[4] = { m_tintColor.x, m_tintColor.y, m_tintColor.z, m_tintColor.w };
    if (ImGui::ColorEdit4("Tint Color", tc))
        m_tintColor = Vector4(tc[0], tc[1], tc[2], tc[3]);

    ImGui::DragFloat("Alpha", &m_alpha, 0.01f, 0.f, 1.f, "%.2f");

    ImGui::SeparatorText("Shader Graph");
    ImGui::InputFloat("Graph ID", &m_graphId, 1.0f, 10.0f, "%.0f");
    m_graphId = std::max(0.0f, m_graphId);
    ImGui::SliderFloat("Graph Metallic", &m_graphMetallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph Roughness", &m_graphRoughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph AO", &m_graphAo, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph Blend", &m_graphBlend, 0.0f, 1.0f);
}

void UIImageComponent::fadeIn(float duration, UIEaseType ease)
{
    UIAnimator::Instance().animateFloat(&m_alpha, 1.f, duration, ease);
}

void UIImageComponent::fadeOut(float duration, UIEaseType ease,
    std::function<void()> onComplete)
{
    UIAnimator::Instance().animateFloat(&m_alpha, 0.f, duration, ease,
        0.f, std::move(onComplete));
}