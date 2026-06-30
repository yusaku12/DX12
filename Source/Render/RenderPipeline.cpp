#include "pch.h"
#include "HiZPyramid.h"
#include "Graphics/GpuDebugMarker.h"
#include "System/DebugPrimitive.h"

namespace
{
    using Clock = std::chrono::high_resolution_clock;
}

namespace
{
    void clearSceneRT()
    {
        auto& dx12 = DX12::Instance();
        auto* cmd = dx12.getGraphicsCommandList();
        dx12.transitionSceneToRenderTarget(cmd);

        FLOAT clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
        cmd->ClearRenderTargetView(dx12.getSceneRTVHandle(), clearColor, 0, nullptr);

        cmd->ClearDepthStencilView(
            dx12.getDSVHandle(),
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            1.0f,
            0,
            0,
            nullptr
        );

        dx12.applySceneRenderTargets(cmd);
        dx12.applyViewportAndScissor(cmd);
    }

    class ShadowMapPass : public RenderPassBase
    {
    public:
        RenderPassId getId()    const override { return RenderPassId::ShadowMap; }
        const char* getName()  const override { return "ShadowMap"; }
        int          getPriority() const override { return -10; }
        RenderPassStage getStage() const override { return RenderPassStage::Scene; }

        bool isEnabled(const RenderPassContext& context) const override
        {
            return context.renderPath == RenderPath::Deferred
                && HasRenderPass(context.passMask, RenderPassFlags::ShadowMap);
        }

        void execute(RenderPassContext&) override
        {
            auto* cmd = DX12::Instance().getGraphicsCommandList();
            if (!cmd) return;

            auto& shadow = ShadowMapRenderer::Instance();

            // 光源方向を DeferredRenderer から同期
            shadow.update(DeferredRenderer::Instance().getLightDirection());

            // カスケード描画
            for (int cascade = 0; cascade < ShadowMapRenderer::CascadeCount; ++cascade)
            {
                shadow.beginCascadePass(cmd, cascade);

                // 光源 VP を ShadowDepth ルートパラメータ 0 にバインド
                cmd->SetGraphicsRootSignature(
                    RootSignatureManager::Instance().getRootSignature(RootSignatureType::ShadowDepth));
                cmd->SetGraphicsRootConstantBufferView(0, shadow.getLightVPCBAddress(cascade));

                //RenderManager::Instance().renderShadowCasters(shadow.getCascadeOBB(cascade));
            }

            // DeferredLighting で使用できるよう SRV へ遷移
            shadow.transitionToSRV(cmd);
        }
    };

    class GBufferPass : public RenderPassBase
    {
    public:
        RenderPassId getId() const override { return RenderPassId::GBuffer; }
        const char* getName() const override { return "GBuffer"; }
        int getPriority() const override { return 0; }
        RenderPassStage getStage() const override { return RenderPassStage::Scene; }

        bool isEnabled(const RenderPassContext& context) const override
        {
            return context.renderPath == RenderPath::Deferred
                && HasRenderPass(context.passMask, RenderPassFlags::GBuffer);
        }

        void execute(RenderPassContext& context) override
        {
            auto* cmd = DX12::Instance().getGraphicsCommandList();
            if (!cmd) return;

            auto& gbuffer = GBufferRenderTargets::Instance();
            gbuffer.transitionToRenderTarget(cmd);
            gbuffer.clear(cmd);
            gbuffer.setRenderTargets(cmd, DX12::Instance().getDSVHandle());
            DX12::Instance().applyViewportAndScissor(cmd);

            if (context.useMultiThreaded)
                RenderManager::Instance().renderMultiThreadedGBuffer();
            else
                RenderManager::Instance().renderGBuffer();
        }
    };

    class ForwardScenePass : public RenderPassBase
    {
    public:
        RenderPassId getId() const override { return RenderPassId::ForwardScene; }
        const char* getName() const override { return "ForwardScene"; }
        int getPriority() const override { return 0; }
        RenderPassStage getStage() const override { return RenderPassStage::Scene; }

        bool isEnabled(const RenderPassContext& context) const override
        {
            return context.renderPath == RenderPath::Forward
                && HasRenderPass(context.passMask, RenderPassFlags::Forward);
        }

        void execute(RenderPassContext& context) override
        {
            if (context.useMultiThreaded)
                RenderManager::Instance().renderMultiThreaded();
            else
                RenderManager::Instance().render();
        }
    };

    class LightingPass : public RenderPassBase
    {
    public:
        RenderPassId getId() const override { return RenderPassId::Lighting; }
        const char* getName() const override { return "Lighting"; }
        int getPriority() const override { return 0; }
        RenderPassStage getStage() const override { return RenderPassStage::BeforePostEffect; }

        bool isEnabled(const RenderPassContext& context) const override
        {
            return context.renderPath == RenderPath::Deferred;
        }

        void execute(RenderPassContext& context) override
        {
            if (HasRenderPass(context.passMask, RenderPassFlags::Lighting)
                && HasRenderPass(context.passMask, RenderPassFlags::GBuffer))
            {
                DeferredRenderer::Instance().renderLighting();
                return;
            }

            clearSceneRT();
        }
    };

    class ForwardPass : public RenderPassBase
    {
    public:
        RenderPassId getId() const override { return RenderPassId::Forward; }
        const char* getName() const override { return "Forward"; }
        int getPriority() const override { return 10; }
        RenderPassStage getStage() const override { return RenderPassStage::BeforePostEffect; }

        bool isEnabled(const RenderPassContext& context) const override
        {
            return context.renderPath == RenderPath::Deferred
                && HasRenderPass(context.passMask, RenderPassFlags::Forward);
        }

        void execute(RenderPassContext& context) override
        {
            if (context.useMultiThreaded)
            {
                RenderManager::Instance().renderMultiThreadedForward();
                context.requestWorkerFlush = true;
            }
            else
            {
                RenderManager::Instance().renderForward();
            }
        }
    };

    class DebugPass : public RenderPassBase
    {
    public:
        RenderPassId getId() const override { return RenderPassId::Debug; }
        const char* getName() const override { return "Debug"; }
        int getPriority() const override { return 100; }
        RenderPassStage getStage() const override { return RenderPassStage::BeforePostEffect; }

        bool isEnabled(const RenderPassContext& context) const override
        {
            return HasRenderPass(context.passMask, RenderPassFlags::Debug);
        }

        void execute(RenderPassContext&) override
        {
            DebugPrimitive::Instance().render();
        }
    };

    class HiZPass : public RenderPassBase
    {
    public:
        RenderPassId getId() const override { return RenderPassId::HiZPyramid; }
        const char* getName() const override { return "HiZPyramid"; }
        int getPriority() const override { return 50; }
        RenderPassStage getStage() const override { return RenderPassStage::BeforePostEffect; }

        bool isEnabled(const RenderPassContext&) const override
        {
            return HiZPyramid::Instance().isEnabled();
        }

        void execute(RenderPassContext&) override
        {
            auto* cmd = DX12::Instance().getGraphicsCommandList();
            if (!cmd)
            {
                return;
            }

            DX12::Instance().transitionDepthToSRV();
            HiZPyramid::Instance().build(cmd);
            // Restore default depth-write state for passes that continue drawing after Hi-Z.
            DX12::Instance().transitionDepthToWrite();
        }
    };

    class PostEffectPass : public RenderPassBase
    {
    public:
        RenderPassId getId() const override { return RenderPassId::PostEffect; }
        const char* getName() const override { return "PostEffect"; }
        int getPriority() const override { return 0; }
        RenderPassStage getStage() const override { return RenderPassStage::PostEffect; }

        bool isEnabled(const RenderPassContext& context) const override
        {
            return HasRenderPass(context.passMask, RenderPassFlags::PostEffect);
        }

        void execute(RenderPassContext& context) override
        {
            context.finalSrvIndex = PostEffectManager::Instance().execute(context.finalSrvIndex);
        }
    };
}

void RenderPipeline::initialize()
{
    registerPass(DXMem::makeUnique<ShadowMapPass>());
    registerPass(DXMem::makeUnique<GBufferPass>(), { RenderPassId::ShadowMap });
    registerPass(DXMem::makeUnique<ForwardScenePass>());
    registerPass(DXMem::makeUnique<LightingPass>(), { RenderPassId::GBuffer });
    registerPass(DXMem::makeUnique<ForwardPass>(), { RenderPassId::Lighting });
    registerPass(DXMem::makeUnique<HiZPass>(), { RenderPassId::Forward });
    registerPass(DXMem::makeUnique<DebugPass>(), { RenderPassId::HiZPyramid });
    registerPass(DXMem::makeUnique<PostEffectPass>());
}

RenderPassBase* RenderPipeline::registerPass(std::unique_ptr<RenderPassBase> pass, std::vector<RenderPassId> dependencies)
{
    auto* ptr = pass.get();
    m_nodes.push_back({ std::move(pass), std::move(dependencies) });
    return ptr;
}

void RenderPipeline::execute(RenderPassContext& context, RenderPassStage stage)
{
    if (stage == RenderPassStage::Scene)
    {
        beginFrameProfile();
    }

    if (stage == RenderPassStage::BeforePostEffect)
    {
        auto* cmd = DX12::Instance().getGraphicsCommandList();
        if (cmd)
        {
            // BeforePostEffect では Scene RT を基準状態に揃える。
            DX12::Instance().transitionSceneToRenderTarget(cmd);
            DX12::Instance().applySceneRenderTargets(cmd);
            DX12::Instance().applyViewportAndScissor(cmd);
        }
    }

    std::vector<size_t> indices;
    indices.reserve(m_nodes.size());

    std::unordered_map<RenderPassId, size_t> indexMap;

    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].pass->getStage() != stage) continue;
        indexMap[m_nodes[i].pass->getId()] = indices.size();
        indices.push_back(i);
    }

    const size_t count = indices.size();
    if (count == 0) return;

    std::vector<int> indegree(count, 0);
    std::vector<std::vector<size_t>> adjacency(count);

    for (size_t i = 0; i < count; ++i)
    {
        auto& node = m_nodes[indices[i]];
        for (auto dep : node.dependencies)
        {
            auto it = indexMap.find(dep);
            if (it == indexMap.end()) continue;

            ++indegree[i];
            adjacency[it->second].push_back(i);
        }
    }

    std::vector<bool> processed(count, false);

    for (size_t processedCount = 0; processedCount < count; ++processedCount)
    {
        int bestIndex = -1;
        int bestPriority = INT_MAX;

        for (size_t i = 0; i < count; ++i)
        {
            if (processed[i] || indegree[i] != 0) continue;

            int priority = m_nodes[indices[i]].pass->getPriority();
            if (priority < bestPriority)
            {
                bestPriority = priority;
                bestIndex = static_cast<int>(i);
            }
        }

        if (bestIndex < 0)
            break;

        processed[bestIndex] = true;

        auto& pass = m_nodes[indices[bestIndex]].pass;
        if (pass->isEnabled(context))
        {
            auto* cmd = DX12::Instance().getGraphicsCommandList();
            GpuDebugMarker::ScopedEvent marker(cmd, pass->getName());
            auto gpuToken = TimeManager::Instance().beginGpuScope(cmd, pass->getName());

            const auto start = Clock::now();
            pass->execute(context);
            const auto end = Clock::now();
            const float ms = std::chrono::duration<float, std::milli>(end - start).count();

            TimeManager::Instance().endGpuScope(cmd, gpuToken);

            const int idx = toIndex(pass->getId());
            if (idx >= 0 && idx < PassCount)
            {
                auto& timing = m_passTimings[idx];
                timing.name = pass->getName();
                timing.frameMs += ms;
                timing.executed = true;
            }
        }

        for (auto next : adjacency[bestIndex])
        {
            --indegree[next];
        }
    }

    if (stage == RenderPassStage::PostEffect)
    {
        endFrameProfile();
    }
}

int RenderPipeline::toIndex(RenderPassId id)
{
    return static_cast<int>(id);
}

void RenderPipeline::beginFrameProfile()
{
    m_profileFrameOpen = true;
    for (auto& timing : m_passTimings)
    {
        timing.frameMs = 0.0f;
        timing.executed = false;
    }
}

void RenderPipeline::endFrameProfile()
{
    if (!m_profileFrameOpen)
    {
        return;
    }

    for (auto& timing : m_passTimings)
    {
        if (!timing.executed)
        {
            timing.lastMs = 0.0f;
            continue;
        }

        timing.lastMs = timing.frameMs;
        if (timing.emaMs <= 0.0f)
        {
            timing.emaMs = timing.frameMs;
        }
        else
        {
            timing.emaMs = timing.emaMs * 0.9f + timing.frameMs * 0.1f;
        }
    }

    m_profileFrameOpen = false;
}

void RenderPipeline::debugImgui()
{
    if (!ImGui::Begin("Render Passes"))
    {
        ImGui::End();
        return;
    }

    renderDebugContents();

    ImGui::End();
}

void RenderPipeline::renderDebugContents()
{

    ImGui::Text("Per-pass CPU timing (ms)");
    ImGui::Separator();

    float totalLast = 0.0f;
    for (const auto& timing : m_passTimings)
    {
        totalLast += timing.lastMs;
    }

    ImGui::Text("Total: %.3f ms", totalLast);
    ImGui::Separator();

    for (const auto& timing : m_passTimings)
    {
        if (timing.name.empty())
        {
            continue;
        }

        ImGui::Text("%-14s  last: %7.3f  avg: %7.3f",
            timing.name.c_str(),
            timing.lastMs,
            timing.emaMs);
    }
}