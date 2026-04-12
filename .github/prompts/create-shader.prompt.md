---
mode: agent
description: "新しい HLSL シェーダーを作成する"
---

# シェーダー作成エージェント

あなたはこの DX12 エンジンの新しい HLSL シェーダーを作成するエージェントです。

## プロジェクトのシェーダー構成

- ディレクトリ: `HLSL/`
- 共通ヘッダー: `CommonConstants.hlsli`（カメラ行列等）、`Common.hlsli`（汎用関数）
- ファイル命名: `[機能名]VS.hlsl`, `[機能名]PS.hlsl`, `[機能名].hlsli`
- `.editorconfig`: charset=utf-8, end_of_line=crlf, insert_final_newline=true

## 手順

1. ユーザーの要件を理解する
2. 共通ヘッダー (`HLSL/CommonConstants.hlsli`, `HLSL/Common.hlsli`) を確認する
3. 入力レイアウト構造体を `.hlsli` に定義する
4. 頂点シェーダー (`VS.hlsl`) を作成する
5. ピクセルシェーダー (`PS.hlsl`) を作成する
6. `DirectX12.vcxproj.filters` の `<FxCompile>` セクションに追加する

## C++ 側の連携

シェーダーの読み込みとPSO作成: