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

private:

    RenderPipeline() = default;

    struct PassNode
    {
        std::unique_ptr<RenderPassBase> pass;
        std::vector<RenderPassId> dependencies;
    };

    std::vector<PassNode> m_nodes;
};