# PostEffect システム

DirectX 12 ベースのフルスクリーンポストエフェクトシステム。  
Unity の **Volume** に相当する `PostEffectComponent` を中心に、ピンポンレンダーターゲットを用いてエフェクトチェーンを実行します。

---

## クラス詳細

### `PostEffectComponent`

`Component` を継承したコンポーネント。  
`RenderManager` のマルチスレッド描画には乗らず、シーンの `draw()` から直接呼び出します。

| メソッド | 説明 |
|---|---|
| `addEffect<T>(args...)` | エフェクトを追加・初期化（同一型の重複は不可） |
| `getEffect<T>()` | 登録済みエフェクトを型で取得 |
| `removeEffect<T>()` | エフェクトを削除 |
| `execute(sceneSrvIndex)` | エフェクトチェーンを実行し最終 SRV インデックスを返す |
| `hasActiveEffects()` | 有効なエフェクトが存在するか確認 |

エフェクトは **`priority`（小さいほど先）** の昇順に自動ソートされます。

---

### `PostEffectBase`

個別エフェクトは必ずこのクラスを継承します。

| 純粋仮想メソッド | 説明 |
|---|---|
| `initialize()` | PSO 登録（起動時1回呼ばれる） |
| `render(cmd, inputSrvIndex)` | フルスクリーン描画 |
| `getName()` | エフェクト名（ImGui 表示用） |
| `getPixelShaderID()` | 使用するピクセルシェーダー ID |

#### 派生クラスで使えるヘルパー

| メソッド | 説明 |
|---|---|
| `registerPSO(ShaderID)` | `RootSignatureType::PostEffect` で PSO 登録、`m_psoKey` に格納 |
| `registerPSO(ShaderID, RootSignatureType)` | RS 種別を指定して PSO 登録、キーを返す（複数パス用） |
| `applyPSO(cmd)` | `m_psoKey` の PSO をコマンドリストにセット |
| `applyPSO(key, cmd)` | 指定キーの PSO をセット（複数パスエフェクト用） |
| `drawFullscreenTriangle(cmd)` | 頂点バッファなしでフルスクリーン三角形を描画 |

---

### `PostEffectRenderTargets`

ピンポン方式（2枚の RT を交互に使用）でエフェクトチェーンを実現するシングルトン。

| メソッド | 説明 |
|---|---|
| `initialize()` | `DX12::initialize()` の後に呼ぶ |
| `resize(width, height)` | ウィンドウリサイズ時に呼ぶ |
| `reset(sceneSrvIndex)` | フレーム開始時に入力 SRV をリセット |
| `swap()` | 書き込み先と入力をスワップ |
| `getFinalOutputSrvIndex()` | 最終出力の SRV インデックスを取得 |
| `getCurrentRTV()` | 現在の書き込み先 RTV ハンドルを取得 |
| `getCurrentInputSrvIndex()` | 現在の入力 SRV インデックスを取得 |
| `transitionWriteToRenderTarget(cmd)` | 書き込み先を `D3D12_RESOURCE_STATE_RENDER_TARGET` に遷移 |
| `transitionWriteToSRV(cmd)` | 書き込み先を `D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE` に遷移 |

---

## `execute()` の内部フロー
