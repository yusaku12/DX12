#pragma once

#include "Component.h"
#include "System/BehaviorTreeRuntime.h"

class BehaviorTreeComponent : public Component
{
public:

    BehaviorTreeComponent() = default;
    ~BehaviorTreeComponent() override = default;

    void update() override;
    void inspectGUI() override;

    void setAssetPath(const std::string& path);
    const std::string& getAssetPath() const { return m_assetPath; }

    void setTreeEnabled(bool enabled) { m_treeEnabled = enabled; }
    bool isTreeEnabled() const { return m_treeEnabled; }

    void setTickInterval(float interval) { m_tickInterval = std::max(interval, 0.01f); }
    float getTickInterval() const { return m_tickInterval; }

    bool reloadAsset();

private:

    std::string m_assetPath;
    bool m_treeEnabled = true;
    float m_tickInterval = 0.1f;
    float m_tickTimer = 0.0f;

    BehaviorTreeRuntime::Asset m_asset;
    BehaviorTreeRuntime::Context m_context;
};
