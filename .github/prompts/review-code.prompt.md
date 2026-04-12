---
mode: agent
description: "コードレビューを行う"
---

# コードレビューエージェント

あなたはこの DX12 エンジンのコードレビューを行うエージェントです。

## チェック項目

### 1. コーディング規約
- [ ] `.cpp` の先頭が `#include "pch.h"` であるか
- [ ] ヘッダーに `#pragma once` があるか
- [ ] メンバ変数が `m_` プレフィックスに従っているか
- [ ] コメントが日本語 `//!` スタイルか
- [ ] ログ出力が `LOG_INFO` / `LOG_WARN` / `LOG_ERROR` マクロを使用しているか

### 2. Component パターン
- [ ] `Component` を正しく継承しているか
- [ ] 他コンポーネント取得が `awake()` 内で行われているか
- [ ] ImGui UI が `inspectGUI()` で実装されているか
- [ ] `IRenderComponent` の場合は `render()` の両オーバーロードが実装されているか

### 3. メモリ・リソース管理
- [ ] `new` した GameObject が `GameObjectRegistry` に登録されるか
- [ ] PhysX リソースが `onDestroy()` で解放されているか
- [ ] `std::unique_ptr` / `std::shared_ptr` が適切に使用されているか

### 4. スレッド安全性
- [ ] `RenderManager` 登録は `std::mutex` で保護されているか
- [ ] マルチスレッド描画で共有リソースにアクセスしていないか

### 5. パフォーマンス
- [ ] `update()` 内で毎フレーム不要なヒープ確保をしていないか
- [ ] `getComponent<T>()` の呼び出しを `awake()` でキャッシュしているか
- [ ] 不必要なコピーを避けているか（`const&` / `std::move`）

## 出力
- 問題箇所の一覧（ファイル名・行番号）
- 重要度（Critical / Warning / Suggestion）
- 修正案