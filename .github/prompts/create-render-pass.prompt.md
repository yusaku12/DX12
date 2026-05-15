---
mode: agent
description: "新しい RenderPass を追加する"
---

# RenderPass 作成エージェント

あなたはこの DX12 エンジンの RenderPipeline に新しいパスを追加するエージェントです。

## 手順

1. 要件を整理
2. `RenderPassId` / `RenderPassFlags` を更新
3. `RenderPipeline.cpp` にパスを追加
4. 依存関係を登録

## 更新箇所

### RenderPassId
- `Source/Render/RenderPassBase.h` の `enum class RenderPassId` に追加

### RenderPassFlags（必要な場合）
- `Source/Camera/CameraComponent.h` の `RenderPassFlags` に追加
- `HasRenderPass()` を使って判定

### RenderPipeline 追加
- `Source/Render/RenderPipeline.cpp` の匿名名前空間に `RenderPassBase` 派生クラスを追加
- `getStage()` は `RenderPassStage::Scene / BeforePostEffect / PostEffect` から選択
- `execute()` 内で `DX12::Instance()` の RT 設定/バリアを行う
- `RenderPipeline::initialize()` で `registerPass()` に追加（必要なら依存関係も指定）

## 出力

- 追加したパスクラス
- 変更点（RenderPassId / RenderPassFlags / RenderPipeline）