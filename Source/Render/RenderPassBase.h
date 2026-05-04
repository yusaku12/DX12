#pragma once

#include "Camera/CameraComponent.h"

//=====================================================
//! RenderPass ID
//=====================================================
enum class RenderPassId : int
{
    GBuffer,
    Lighting,
    ForwardScene,
    Forward,
    Debug,
    PostEffect
};

//=====================================================
//! RenderPass の実行段階
//=====================================================
enum class RenderPassStage : int
{
    Scene = 0,
    BeforePostEffect,
    PostEffect
};

//=====================================================
//! RenderPass 実行コンテキスト
//=====================================================
struct RenderPassContext
{
    RenderPath renderPath = RenderPath::Deferred;
    RenderPassFlags passMask = RenderPassFlags::None;
    bool useMultiThreaded = true;
    UINT sceneSrvIndex = 0;
    UINT finalSrvIndex = 0;
    bool requestWorkerFlush = false;
};

//=====================================================
//! RenderPass 基底クラス
//=====================================================
class RenderPassBase
{
public:

    virtual ~RenderPassBase() = default;

    //! パスID
    virtual RenderPassId getId() const = 0;

    //! パス名
    virtual const char* getName() const = 0;

    //! 優先度（小さいほど先に実行）
    virtual int getPriority() const = 0;

    //! 実行段階
    virtual RenderPassStage getStage() const = 0;

    //! 実行可能か
    virtual bool isEnabled(const RenderPassContext& context) const = 0;

    //! 実行
    virtual void execute(RenderPassContext& context) = 0;
};