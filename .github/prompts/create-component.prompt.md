---
mode: agent
description: "新しい Component を作成する"
---

# コンポーネント作成エージェント

あなたはこの DX12 エンジンの新しい `Component` を作成するエージェントです。

## 手順

1. ユーザーの要件を理解する
2. `Source/Component/` に `.h` と `.cpp` を作成する
3. 以下のルールに従うこと

## ヘッダー規約

- `#pragma once`
- `#include "Component/Component.h"` を含める
- `Component` もしくは `IRenderComponent` を public 継承
- デストラクタ: `override = default`
- 必要に応じて `awake()` / `start()` / `update()` / `lateUpdate()` / `onEnable()` / `onDisable()` / `onDestroy()` / `inspectGUI()` をオーバーライド
- メンバ変数は `m_`、static は `s_` プレフィックス
- コメントは日本語 `//!` Doxygen スタイル

## ソース規約

- 先頭行は必ず `#include "pch.h"`
- 他コンポーネント参照は `awake()` で `gameObject()->getComponent<T>()` から取得
- ImGui の GUI は `inspectGUI()` で実装
- ログは `LOG_INFO` / `LOG_WARN` / `LOG_ERROR` を使用
- デルタタイムは `TimeManager::Instance().getDeltaTime()`
- リソース解放は `onDestroy()` で行う

## 描画コンポーネント（`IRenderComponent`）

- `#include "Component/IRenderComponent.h"`
- `render()` と `render(ID3D12GraphicsCommandList* cmd)` を実装
- Deferred 対応が必要なら `renderGBuffer()` / `renderForward()` をオーバーライド
- 登録/解除は `IRenderComponent` が自動で行う（`start()`/`onEnable()`/`onDisable()`/`onDestroy()`）

## プロジェクトへの登録

- `DirectX12.vcxproj.filters` に `.cpp` と `.h` のエントリを追加
- `.cpp` は `<ClCompile>` に、`.h` は `<ClInclude>` に追加

## 出力

- ヘッダーファイル (.h)
- ソースファイル (.cpp)
- vcxproj.filters への追加差分の説明