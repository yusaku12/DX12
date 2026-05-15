# Copilot カスタム指示 — DX12 ゲームエンジン

## プロジェクト概要
DirectX 12 ベースの自作ゲームエンジン。C++20 準拠。Unity 風のコンポーネントアーキテクチャを採用。

## アーキテクチャ

### コアクラス階層
- `Object` — 全エンジンオブジェクトの基底（インスタンスID・名前）
- `Component : Object` — ライフサイクル: `awake()` → `start()` → `update()` → `lateUpdate()`
- `GameObject : Object` — `Component` を束ねるコンテナ。`addComponent<T>()` / `getComponent<T>()`
- `IRenderComponent : Component` — 描画可能コンポーネントの共通インターフェース

### シングルトンマネージャ群（`static T& Instance()`）
- `GameObjectRegistry` — 全 GameObject の更新・破棄
- `SceneManager` — シーン登録・切り替え
- `RenderManager` — 描画コンポーネント登録・シングル/マルチスレッド描画
- `RenderPipeline` — RenderPass の実行管理
- `ShaderManager` — HLSL ホットリロード + shaderTable 管理
- `PSOCreator` — PSO キャッシュ/生成
- `RootSignatureManager` — RootSignature 管理
- `PostEffectManager` — Volume 実行管理
- `TimeManager` — デルタタイム・FPS・プロファイラ
- `InputManager` — キーボード・マウス・アクションマッピング・軸入力
- `AudioManager` — サウンド読み込み・再生
- `PhysicsWorld` — PhysX 5.x ラッパー・シミュレーション
- `EditorManager` — ImGui エディタ UI
- `CameraManager` — カメラ登録・アクティブカメラ管理
- `DebugPrimitive` — デバッグ用ワイヤーフレーム描画

### 主要コンポーネント
- `TransformComponent` — 位置 / 回転(Quaternion) / スケール / ローカル&ワールド行列
- `FbxRenderComponent : IRenderComponent` — FBX モデル描画（ボーン対応）
- `AnimationComponent` — アニメーション再生・クロスフェード・シーケンサー
- `RigidbodyComponent` — PhysX 剛体（Dynamic/Kinematic/Static）
- `ColliderComponent` — 衝突形状（Box/Sphere/Capsule/Plane）
- `CameraComponent` — カメラ基底（ビュー行列・プロジェクション行列）
- `FreeCameraComponent : CameraComponent` — WASD + マウスのフリーカメラ
- `PostEffectComponent` — Volume（PostEffect チェーン）
- `SkyboxComponent` — スカイボックス描画

### RenderPipeline
- `RenderPassBase` を `RenderPipeline` に登録して実行
- `RenderPath`: `Deferred` / `Forward`
- `RenderPassFlags` で描画パスの有効/無効を切り替え
- `RenderPipeline::execute(context, stage)` でパス実行

### シェーダー
- HLSL ソース: `HLSL/`
- コンパイル済み: `Shader/`
- `ShaderData.h` の `ShaderID` と `shaderTable` は必ず一致

### ポストエフェクト
- `PostEffectManager` が Volume を優先度順で実行
- `PostEffectComponent` が `PostEffectBase` を保持しチェーン実行

## コーディング規約（必須）

- **全 .cpp の先頭に `#include "pch.h"`**
- ヘッダーガードは `#pragma once`
- メンバ変数: `m_` / static: `s_`
- コメント: 日本語、`//!` Doxygen
- ログ: `LOG_INFO()` / `LOG_WARN()` / `LOG_ERROR()`
- `using namespace DirectX; using namespace SimpleMath;` は `pch.h` で宣言済み

### コンポーネント作成パターン
- `awake()` で他コンポーネントをキャッシュ
- `start()` で登録・初期化
- `onEnable()` / `onDisable()` でマネージャ登録切り替え
- `onDestroy()` でリソース解放
- `inspectGUI()` で ImGui 表示

### シーン作成パターン
- `onEnter()` で `GameObject` を生成
- `update()` でロジック更新
- `debugDraw()` は必ず実装
- `draw()` を上書きする場合は `Scene::draw()` を呼び RenderPipeline を維持