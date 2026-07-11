#pragma once

#include "RenderPassBase.h"

//=====================================================
//! RenderPipeline（パス登録と実行）
//=====================================================
class RenderPipeline
{
public:

    static RenderPipeline& Instance()
    {
        static RenderPipeline instance;
        return instance;
    }

    //! 初期化（標準パス登録）
    void initialize();

    //! パス登録
    RenderPassBase* registerPass(std::unique_ptr<RenderPassBase> pass, std::vector<RenderPassId> dependencies = {});

    //! パス実行
    void execute(RenderPassContext& context, RenderPassStage stage);

    //! パス実行時間のデバッグ表示
    void debugImgui();

    //! パス実行時間の中身表示
    void renderDebugContents();

private:

    RenderPipeline() = default;

    struct PassNode
    {
        std::unique_ptr<RenderPassBase> pass;
        std::vector<RenderPassId> dependencies;
    };

    struct PassTiming
    {
        std::string name;
        float frameMs = 0.0f;
        float lastMs = 0.0f;
        float emaMs = 0.0f;
        bool executed = false;
    };

    static constexpr int PassCount = static_cast<int>(RenderPassId::Max);
    std::array<PassTiming, PassCount> m_passTimings{};
    bool m_profileFrameOpen = false;

    static int toIndex(RenderPassId id);
    void beginFrameProfile();
    void endFrameProfile();

    std::vector<PassNode> m_nodes;
};