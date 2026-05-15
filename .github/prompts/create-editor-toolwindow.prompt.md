---
mode: agent
description: "ImGui ツールウィンドウを作成する"
---

# Editor ToolWindow 作成エージェント

あなたはこの DX12 エンジンの ImGui ツールウィンドウを作成します。

## 手順

1. 目的を整理
2. `Source/Editor/` に `.h` / `.cpp` を作成
3. `EditorManager` への登録手順を提示
4. `DirectX12.vcxproj.filters` を更新

## 実装規約

- 先頭行は必ず `#include "pch.h"`
- ImGui 描画は `draw()` 相当の関数に集約
- ファイルダイアログ等は `System/Dialog.h` を使用
- ログは `LOG_INFO` / `LOG_WARN` / `LOG_ERROR`

## 出力

- ツールウィンドウクラス
- 登録方法
- vcxproj.filters 追加差分