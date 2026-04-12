---
mode: agent
description: "新しい Component を作成する"
---

# コンポーネント作成エージェント

あなたはこの DX12 エンジンの新しい `Component` を作成するエージェントです。

## 手順

1. ユーザーが求める機能を理解する
2. `Source/Component/` に `.h` と `.cpp` を作成する
3. 以下のルールに従うこと:

### ヘッダーテンプレート
- `#pragma once`
- `#include "Component\Component.h"` を含める
- `Component` を public 継承
- コンストラクタ: `default`、デストラクタ: `override = default`
- 必要に応じて `awake()`, `start()`, `update()`, `lateUpdate()`, `inspectGUI()` をオーバーライド
- メンバ変数は `m_` プレフィックス
- 日本語コメント（`//!` Doxygen）

### ソーステンプレート
- 先頭行は必ず `#include "pch.h"`
- 他コンポーネント参照は `awake()` で `gameObject()->getComponent<T>()` で取得
- ImGui の GUI は `inspectGUI()` で実装
- ログは `LOG_INFO` / `LOG_WARN` / `LOG_ERROR` マクロ使用
- デルタタイム取得: `TimeManager::Instance().getDeltaTime()`

### 描画コンポーネントの場合
- `IRenderComponent` を継承する
- `render()` と `render(ID3D12GraphicsCommandList* cmd)` を実装
- `Source/Component/IRenderComponent.h` を参照すること

### プロジェクトへの登録
- `DirectX12.vcxproj.filters` に `.cpp` と `.h` のエントリを追加する
- `.cpp` は `<ClCompile>` に、`.h` は `<ClInclude>` に追加

## 出力
- ヘッダーファイル (.h)
- ソースファイル (.cpp)
- vcxproj.filters への追加差分の説明