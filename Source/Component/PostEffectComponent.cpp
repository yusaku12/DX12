#include "pch.h"
#include "PostEffectComponent.h"
#include "PostEffect\PostEffectRenderTargets.h"

void PostEffectComponent::awake()
{
    LOG_INFO("PostEffectComponent initialized");
}

void PostEffectComponent::inspectGUI()
{
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
        ImGui::Separator();
    }
}

UINT PostEffectComponent::execute(UINT sceneSrvIndex)
{
    if (!isActiveInHierarchy() || !hasActiveEffects())
        return sceneSrvIndex;

    auto* cmd = DX12::Instance().getGraphicsCommandList();
    auto& rt = PostEffectRenderTargets::Instance();

    // DescriptorHeap を設定（ポストパス用に明示的に呼ぶ）
    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);

    rt.reset(sceneSrvIndex);

    for (auto& effect : m_effects)
    {
        if (!effect->isEnabled()) continue;

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
    }

    return rt.getFinalOutputSrvIndex();
}

bool PostEffectComponent::hasActiveEffects() const
{
    return std::any_of(m_effects.begin(), m_effects.end(),
        [](const std::unique_ptr<PostEffectBase>& e) { return e->isEnabled(); });
}

void PostEffectComponent::sortEffects()
{
    std::sort(m_effects.begin(), m_effects.end(),
        [](const std::unique_ptr<PostEffectBase>& a, const std::unique_ptr<PostEffectBase>& b)
        {
            return a->getPriority() < b->getPriority();
        });
}