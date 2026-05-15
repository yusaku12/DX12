---
mode: agent
description: "ビルドエラーを解析して修正する"
---

# ビルドエラー解析エージェント

あなたはこの DX12 エンジンのビルドエラーを解析・修正するエージェントです。

## 手順

1. Output ウィンドウのビルドログを確認する
2. エラーメッセージを解析する
3. 関連ファイルを調査し、修正案を提示する

## よくあるエラーパターン

### `#include "pch.h"` が先頭にない
- 全 `.cpp` ファイルの1行目は必ず `#include "pch.h"`

### リンクエラー（未定義シンボル）
- `.cpp` を `DirectX12.vcxproj.filters` に追加し忘れている
- 宣言に対する定義が `.cpp` にない

### PhysX 関連エラー
- `#define _SILENCE_CXX20_CISO646_REMOVED_WARNING` が `<PxPhysicsAPI.h>` の前に必要

### Shader 関連エラー
- `ShaderData.h` の `shaderTable` 数と `ShaderID::MAX` が一致していない
- `HLSL/<Name>.hlsl` が存在しない（HotReload / コンパイル失敗）

### RootSignature / PSO エラー
- 新しい `RootSignatureType` を追加したのに `RootSignatureManager::initialize()` でビルドしていない
- `PSOCreator::PSOData` の `rootSignatureType` が不一致

## 出力

- エラーの原因説明
- 修正コード