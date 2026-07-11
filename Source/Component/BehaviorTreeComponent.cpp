#include "pch.h"
#include "BehaviorTreeComponent.h"

namespace
{
    constexpr float kFixedStepSeconds = 1.0f / 60.0f;
}

void BehaviorTreeComponent::update()
{
    if (!m_treeEnabled || m_asset.nodes.empty() || !gameObject())
    {
        return;
    }

    const float stepDt = kFixedStepSeconds;

    m_tickTimer -= stepDt;
    if (m_tickTimer > 0.0f)
    {
        return;
    }

    m_tickTimer = m_tickInterval;
    BehaviorTreeRuntime::tick(m_asset, gameObject(), m_context, stepDt);
}

void BehaviorTreeComponent::inspectGUI()
{
    char pathBuffer[260]{};
    if (!m_assetPath.empty())
    {
        const size_t copySize = std::min(m_assetPath.size(), sizeof(pathBuffer) - 1);
        std::memcpy(pathBuffer, m_assetPath.data(), copySize);
        pathBuffer[copySize] = '\0';
    }

    if (ImGui::InputText("BT Asset", pathBuffer, sizeof(pathBuffer)))
    {
        setAssetPath(pathBuffer);
    }

    ImGui::Checkbox("Tree Enabled", &m_treeEnabled);
    ImGui::DragFloat("Tick Interval", &m_tickInterval, 0.01f, 0.01f, 1.0f);

    if (ImGui::Button("Reload BT"))
    {
        reloadAsset();
    }

    ImGui::Text("Nodes: %d", static_cast<int>(m_asset.nodes.size()));
}

void BehaviorTreeComponent::setAssetPath(const std::string& path)
{
    m_assetPath = path;
    reloadAsset();
}

bool BehaviorTreeComponent::reloadAsset()
{
    m_asset = {};
    if (m_assetPath.empty())
    {
        return false;
    }

    const bool ok = BehaviorTreeRuntime::loadAsset(std::filesystem::path(m_assetPath), m_asset);
    if (!ok)
    {
        LOG_WARN("[BehaviorTreeComponent] Failed to load tree asset: %s", m_assetPath.c_str());
        m_treeEnabled = false;
    }

    return ok;
}