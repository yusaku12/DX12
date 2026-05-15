---
mode: agent
description: "PostEffect の設計・実装を行う"
---

# PostEffect システム

DirectX 12 ベースのフルスクリーンポストエフェクトシステム。  
`PostEffectComponent`（Unity の Volume 相当）を中心に、ピンポン RT を用いてエフェクトチェーンを実行します。

## 関連クラス

- `PostEffectManager` — Volume コンポーネントの登録・優先度ソート・実行
- `PostEffectComponent` — Volume 本体（複数エフェクトの保持）
- `PostEffectBase` — 各エフェクトの基底
- `PostEffectRenderTargets` — ピンポン RT 管理

## PostEffectComponent の要点

- `addEffect<T>()` / `getEffect<T>()` / `removeEffect<T>()`
- `execute()` / `executeChain(weight)`
- `setWeight()` / `setBlendDistance()` / `setGlobal()` / `setVolumePriority()`
- `requiresDepth()` が true の場合は深度 SRV を要求

## PostEffectBase の要点

- `initialize()` / `render(cmd, inputSrvIndex)` / `getName()` / `getPixelShaderID()`
- `registerPSO()` / `applyPSO()` / `drawFullscreenTriangle()`

## 実行フロー

1. `PostEffectManager::execute()` が Volume を優先度順に実行
2. `PostEffectComponent::executeChain()` が有効エフェクトのみ実行
3. `PostEffectRenderTargets` が入力/出力をピンポンで切替

## 単一パスエフェクト（例）

- `initialize()` で `registerPSO(ShaderID::XxxPS)`
- `render()` で CBV/SRV をセットし `drawFullscreenTriangle()`

## 複数パスエフェクトの注意

- 中間 RT は自前で作成し、PostEffect RT には触れない
- 最終パスは **必ず `OMSetRenderTargets` を再セット**
- `swap()` は `execute()` 側に任せる

## 注意事項

- `PostEffectRenderTargets::initialize()` は `DX12::initialize()` 後
- `ShaderData.h` の `shaderTable` と `ShaderID::MAX` の数は一致させる
- 新しい `RootSignatureType` 追加時は `RootSignatureManager::initialize()` でビルド

## 出力

- エフェクトクラス（`.h` / `.cpp`）
- 必要な HLSL / ShaderData / PSO 変更