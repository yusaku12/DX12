#include "pch.h"
#include "PostEffectComponent.h"
#include "TransformComponent.h"

void PostEffectComponent::awake()
{
    if (gameObject())
        m_transform = gameObject()->getComponent<TransformComponent>();

    LOG_INFO("PostEffectComponent initialized");
}

void PostEffectComponent::onEnable()
{
    if (!m_transform && gameObject())
        m_transform = gameObject()->getComponent<TransformComponent>();

    registerToManager();
}

void PostEffectComponent::onDisable()
{
    unregisterFromManager();
}

void PostEffectComponent::onDestroy()
{
    unregisterFromManager();
}

void PostEffectComponent::inspectGUI()
{
    ImGui::Text("Post Effect Volume");
    ImGui::Separator();

    ImGui::DragInt("Priority", &m_volumePriority, 1.0f, -100, 100);

    float weight = m_weight;
    if (ImGui::DragFloat("Weight", &weight, 0.01f, 0.0f, 1.0f))
        setWeight(weight);

    ImGui::Checkbox("Global", &m_isGlobal);

    if (!m_isGlobal)
    {
        float blendDistance = m_blendDistance;
        if (ImGui::DragFloat("Blend Distance", &blendDistance, 0.1f, 0.0f, 1000.0f))
            setBlendDistance(blendDistance);
    }

    ImGui::Separator();

    ImGui::Text("Post Effect Stack (%zu)", m_effects.size());
    ImGui::Separator();

    for (size_t i = 0; i < m_effects.size(); ++i)
    {
        auto& effect = m_effects[i];
        ImGui::PushID(static_cast<int>(i));

        bool enabled = effect->isEnabled();
        if (ImGui::Checkbox(effect->getName(), &enabled))
        {
            effect->setEnabled(enabled);
        }

        if (enabled)
        {
            ImGui::Indent();
            effect->inspectGUI();
            ImGui::Unindent();
        }

        ImGui::PopID();
    }
}

UINT PostEffectComponent::execute(UINT sceneSrvIndex)
{
    auto& rt = PostEffectRenderTargets::Instance();
    rt.reset(sceneSrvIndex);

    bool executed = executeChain(1.0f);
    return executed ? rt.getFinalOutputSrvIndex() : sceneSrvIndex;
}

bool PostEffectComponent::executeChain(float volumeWeight)
{
    if (!isActiveInHierarchy() || !hasActiveEffects() || volumeWeight <= 0.0f)
        return false;

    auto* cmd = DX12::Instance().getGraphicsCommandList();
    auto& rt = PostEffectRenderTargets::Instance();

    // DescriptorHeap を設定（ポストパス用に明示的に呼ぶ）
    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);

    bool executed = false;

    for (auto& effect : m_effects)
    {
        if (!effect->isEnabled()) continue;

        effect->setBlendWeight(volumeWeight);

        // 書き込み先を RENDER_TARGET に遷移
        rt.transitionWriteToRenderTarget(cmd);

        // ビューポート・シザー設定
        DX12::Instance().applyViewportAndScissor(cmd);

        // RTV をセット（深度テストなし）
        auto rtvHandle = rt.getCurrentRTV();
        cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        // エフェクト描画
        effect->render(cmd, rt.getCurrentInputSrvIndex());

        // SRV に遷移してスワップ
        rt.transitionWriteToSRV(cmd);
        rt.swap();

        executed = true;
    }

    return executed;
}

bool PostEffectComponent::hasActiveEffects() const
{
    return std::any_of(m_effects.begin(), m_effects.end(),
        [](const std::unique_ptr<PostEffectBase>& e) { return e->isEnabled(); });
}

void PostEffectComponent::setWeight(float weight)
{
    m_weight = std::clamp(weight, 0.0f, 1.0f);
}

void PostEffectComponent::setBlendDistance(float distance)
{
    m_blendDistance = std::max(distance, 0.0f);
}

float PostEffectComponent::computeBlendWeight(const Vector3& cameraPos) const
{
    if (!isActiveInHierarchy())
        return 0.0f;

    float base = std::clamp(m_weight, 0.0f, 1.0f);
    if (base <= 0.0f)
        return 0.0f;

    if (m_isGlobal)
        return base;

    if (!m_transform)
        return 0.0f;

    Vector3 diff = cameraPos - m_transform->getPosition();
    float distance = diff.Length();

    if (m_blendDistance <= 0.0f)
        return distance <= 0.0f ? base : 0.0f;

    float t = 1.0f - (distance / m_blendDistance);
    t = std::clamp(t, 0.0f, 1.0f);
    return base * t;
}

void PostEffectComponent::sortEffects()
{
    std::sort(m_effects.begin(), m_effects.end(),
        [](const std::unique_ptr<PostEffectBase>& a, const std::unique_ptr<PostEffectBase>& b)
        {
            return a->getPriority() < b->getPriority();
        });
}

void PostEffectComponent::registerToManager()
{
    if (!m_registered)
    {
        PostEffectManager::Instance().registerComponent(this);
        m_registered = true;
    }
}

void PostEffectComponent::unregisterFromManager()
{
    if (m_registered)
    {
        PostEffectManager::Instance().unregisterComponent(this);
        m_registered = false;
    }
}