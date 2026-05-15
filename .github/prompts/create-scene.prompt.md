---
mode: agent
description: "新しい Scene を作成する"
---

# シーン作成エージェント

あなたはこの DX12 エンジンの新しい `Scene` を作成するエージェントです。

## 手順

1. ユーザーの要件を理解する
2. `Source/Scene/` に `.h` / `.cpp` を作成する
3. `SceneManager` に登録する
4. `DirectX12.vcxproj.filters` に追加する

## シーンクラス作成（`Source/Scene/`）

### ヘッダー規約

- `#pragma once`
- `#include "Scene/Scene.h"`
- `class XxxScene : public Scene`
- `onEnter()` / `update()` / `debugDraw()` をオーバーライド
- 必要に応じて `onExit()` / `draw()` をオーバーライド
- コメントは日本語 `//!`

### ソース規約

- 先頭行は必ず `#include "pch.h"`
- `onEnter()` で `GameObject` を生成し、`addComponent<T>()` で構成
- `update()` でロジック更新、必要なら `DebugPrimitive::Instance()` でデバッグ描画
- `draw()` をオーバーライドする場合は `Scene::draw()` を呼んで RenderPipeline を維持すること
- `onExit()` をオーバーライドする場合は `Scene::onExit()` を呼ぶこと

## SceneManager への登録

- `Source/Scene/SceneManager.h` の `SceneId` に新規IDを追加（`MAX` の前）
- `Source/Scene/SceneManager.cpp` に `#include` を追加
- `SceneManager::initialize()` で `registerScene<NewScene>(SceneId::NewScene)` を追加
- 必要なら初期シーンの `loadScene()` を更新

## プロジェクトへの登録

- `DirectX12.vcxproj.filters` に `.cpp` と `.h` を追加

## 出力

- ヘッダー / ソース
- SceneManager への追加差分
- vcxproj.filters への追加差分