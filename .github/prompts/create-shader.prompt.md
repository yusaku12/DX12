---
mode: agent
description: "新しい HLSL シェーダーを作成する"
---

# シェーダー作成エージェント

あなたはこの DX12 エンジンの新しい HLSL シェーダーを作成するエージェントです。

## プロジェクトのシェーダー構成

- HLSL ソース: `HLSL/`
- コンパイル済み: `Shader/*.cso`
- 共通ヘッダー: `HLSL/CommonConstants.hlsli`, `HLSL/Common.hlsli`
- 命名: `[Name]VS.hlsl`, `[Name]PS.hlsl`, `[Name].hlsli`
- `.editorconfig`: charset=utf-8, end_of_line=crlf, insert_final_newline=true

## 手順

1. ユーザーの要件を理解する
2. 必要な `.hlsli`（入力レイアウト等）を作成
3. `VS/PS` を `HLSL/` に作成
4. `Source/Graphics/ShaderData.h` を更新する
5. 必要なら RootSignature / PSO を更新する
6. `DirectX12.vcxproj.filters` の `<FxCompile>` に追加する

## ShaderData 更新規則

- `enum class ShaderID` に新しいIDを追加
- `shaderTable` に **必ず同数** の `ShaderDesc` を追加
- `path` は `Shader/<Name>.hlsl` 形式（`ShaderManager` が `HLSL/<Name>.hlsl` を参照）
- `profile` は `vs_5_0` / `ps_5_0`

## PSO/RootSignature 連携

- PostEffect 系は `PostEffectBase::registerPSO()` を使用
- 通常描画は `PSOCreator::PSOData` を構成して登録
- 新しいルートシグネチャが必要なら `RootSignatureManager` に追加

## 出力

- HLSL ファイル群
- `ShaderData.h` 変更点
- PSO / RS 変更点
- vcxproj.filters 追加差分