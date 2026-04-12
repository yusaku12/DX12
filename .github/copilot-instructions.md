# Copilot カスタム指示 — DX12 ゲームエンジン

## プロジェクト概要
DirectX 12 ベースの自作ゲームエンジン。C++20 準拠。
Unity 風のコンポーネントアーキテクチャを採用。

## アーキテクチャ

### コアクラス階層
- `Object` — 全エンジンオブジェクトの基底（インスタンスID・名前）
- `Component : Object` — ライフサイクル: `awake()` → `start()` → `update()` → `lateUpdate()`
- `GameObject : Object` — `Component` を束ねるコンテナ。`addComponent<T>()` / `getComponent<T>()`
- `IRenderComponent : Component` — 描画可能コンポーネントの共通インターフェース

### シングルトンマネージャ群（`static T& Instance()` パターン）
| クラス | 役割 |
|---|---|
| `GameObjectRegistry` | 全 GameObject の更新・破棄 |
| `SceneManager` | シーン登録・切り替え |
| `RenderManager` | 描画コンポーネント登録・シングル/マルチスレッド描画 |
| `TimeManager` | デルタタイム・FPS・プロファイラ |
| `InputManager` | キーボード・マウス・アクションマッピング・軸入力 |
| `AudioManager` | サウンド読み込み・再生 |
| `PhysicsWorld` | PhysX 5.x ラッパー・シミュレーション |
| `EditorManager` | ImGui エディタ UI |
| `CameraManager` | カメラ登録・アクティブカメラ管理 |
| `DebugPrimitive` | デバッグ用ワイヤーフレーム描画 |

### 主要コンポーネント
- `TransformComponent` — 位置 / 回転(Quaternion) / スケール / ローカル&ワールド行列
- `FbxRenderComponent : IRenderComponent` — FBX モデル描画（ボーン対応）
- `AnimationComponent` — アニメーション再生・クロスフェード・シーケンサー
- `RigidbodyComponent` — PhysX 剛体（Dynamic/Kinematic/Static）
- `ColliderComponent` — 衝突形状（Box/Sphere/Capsule/Plane）
- `CameraComponent` — カメラ基底（ビュー行列・プロジェクション行列）
- `FreeCameraComponent : CameraComponent` — WASD + マウスのフリーカメラ

### シーン
- `Scene` — 基底クラス。`onEnter()` / `update()` / `draw()` / `debugDraw()` / `onExit()`
- シーン登録: `SceneManager::registerScene<T>(SceneId)` + enum `SceneId`

## コーディング規約

### 必須ルール
- プリコンパイル済みヘッダー: **全 .cpp ファイルの先頭に `#include "pch.h"`**
- ヘッダーガード: `#pragma once`
- メンバ変数: `m_` プレフィックス（例: `m_position`）
- static メンバ変数: `s_` プレフィックス（例: `s_idCounter`）
- コメント: 日本語、`//!` Doxygen スタイル
- 名前空間省略: `using namespace DirectX;` `using namespace SimpleMath;`（pch.h で宣言済み）
- ログ出力: `LOG_INFO()` / `LOG_WARN()` / `LOG_ERROR()` マクロ使用

### コンポーネント作成パターン