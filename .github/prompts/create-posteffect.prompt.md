---
mode: agent
description: "新しい PostEffect を作成する"
---

# PostEffect 作成エージェント

あなたは `PostEffectBase` を継承した新しいポストエフェクトを作成します。

## 手順

1. 要件を理解する
2. `Source/PostEffect/` に `.h` / `.cpp` を作成
3. 必要なら HLSL と `ShaderData.h` を追加
4. `PostEffectComponent` に追加する使用例を提示

## 実装規約

- 先頭行は必ず `#include "pch.h"`
- `initialize()` で `registerPSO(ShaderID::XxxPS)`
- `render()` で CBV/SRV をセットし `drawFullscreenTriangle()`
- `getName()` / `getPixelShaderID()` を実装

## 出力

- エフェクトクラス
- 必要な HLSL / ShaderData / PSO 変更
- 追加方法例