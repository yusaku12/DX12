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

メンバ変数:

| 変数 | 説明 |
|---|---|
| `m_enabled` | 有効/無効フラグ |
| `m_priority` | 実行順（小さいほど先） |
| `m_psoKey` | `registerPSO(ShaderID)` で登録されたデフォルト PSO キー |

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
| `transitionWriteToRenderTarget(cmd)` | 書き込み先を `RENDER_TARGET` に遷移 |
| `transitionWriteToSRV(cmd)` | 書き込み先を `PIXEL_SHADER_RESOURCE` に遷移 |

---

## `execute()` の内部フロー
UINT PostEffectComponent::execute(UINT sceneSrvIndex) { if (!isActiveInHierarchy() || !hasActiveEffects()) return sceneSrvIndex;
auto* cmd = DX12::Instance().getGraphicsCommandList();
auto& rt  = PostEffectRenderTargets::Instance();

DescriptorHeapManager::Instance().setDescriptorHeap(cmd); // ヒープをセット
rt.reset(sceneSrvIndex);

for (auto& effect : m_effects)
{
    if (!effect->isEnabled()) continue;

    rt.transitionWriteToRenderTarget(cmd);          // ① PostEffect RT を RT 状態に
    DX12::Instance().applyViewportAndScissor(cmd);  // ② フル解像度 VP / シザー
    auto rtvHandle = rt.getCurrentRTV();
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr); // ③ RT をセット

    effect->render(cmd, rt.getCurrentInputSrvIndex()); // ④ エフェクト描画

    rt.transitionWriteToSRV(cmd); // ⑤ PostEffect RT を SRV に
    rt.swap();                    // ⑥ ピンポンスワップ
}

return rt.getFinalOutputSrvIndex();
---

## エフェクト作成パターン（単一パス）
// MyEffect.h class MyEffect : public PostEffectBase { public: void initialize() override { registerPSO(ShaderIDMyEffectPS); // m_psoKey に自動格納 m_cb = stdmake_unique<ConstantBuffer<CBuffer>>(); }
void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override
{
    // execute() が OMSetRenderTargets・バリア・VP を設定済み
    // → PSO・CBV・SRV をセットして描画するだけでよい
    m_cb->update(m_params);
    applyPSO(cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1,
        DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex));
    drawFullscreenTriangle(cmd);
}

const char* getName()          const override { return "MyEffect"; }
ShaderID    getPixelShaderID() const override { return ShaderID::MyEffectPS; }

---

## 複数パスエフェクト（中間 RT を持つ場合）の実装規則

BloomEffect のように自前の中間 RT を持ち、複数パスで描画するエフェクトの規則です。

### 中間パス（Prefilter / Downsample / Upsample 等）

- 自前の RT / RTV / SRV を `CreateCommittedResource` で作成する
- SRV スロットは `DescriptorHeapManager::Instance().allocateRange()` で確保する
- `transitionToRT()` → `OMSetRenderTargets()` → 描画 → `transitionToSRV()` の順で記述する
- `PostEffectRenderTargets` には **一切触れない**

- 
### 最終パス（PostEffect RT への書き戻し）

`execute()` 側が `transitionWriteToRenderTarget()` を呼んでいるが、  
中間パスで `OMSetRenderTargets` が上書きされているため **必ず再セットが必要**。  
`transitionWriteToSRV()` と `swap()` は **`execute()` 側に任せて呼ばない**。

void passComposite(ID3D12GraphicsCommandList* cmd, UINT sceneSrvIndex, UINT bloomSrvIndex) { // ⚠️ transitionWriteToRenderTarget() は execute() 側で完了済み // ⚠️ 中間パスで OMSetRenderTargets が上書きされているので必ず再セット auto rtv = PostEffectRenderTargets::Instance().getCurrentRTV(); cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
setViewport(cmd, DX12::Instance().getScreenWidth(),
                 DX12::Instance().getScreenHeight());

// ... PSO・CBV・SRV をセット ...
drawFullscreenTriangle(cmd);

// transitionWriteToSRV() / swap() は execute() に任せるため呼ばない


### ❌ やってはいけないこと

| NG パターン | 結果 |
|---|---|
| 最終パス内で `PostEffectRenderTargets::swap()` を呼ぶ | `execute()` と二重スワップになり**空バッファを参照する** |
| 最終パスで `OMSetRenderTargets` を省略する | 中間パスで SRV 状態になった RT に描画しようとして **`INVALID_SUBRESOURCE_STATE` エラー** |
| 中間パスで `PostEffectRenderTargets` の RT に書き込む | ピンポンの状態が崩れて**最終出力が空になる** |

---

## RootSignature の種類

| 種別 | スロット構成 | 用途 |
|---|---|---|
| `PostEffect` | `b0`(CBV) + `t0`(SRV×1) | 単一テクスチャ入力（標準） |
| `BloomComposite` | `b0`(CBV) + `t0`(SRV×1) + `t1`(SRV×1) | 2テクスチャ入力（合成パス等） |

2つ以上の SRV が必要なパスは `BloomComposite` RS を使う。

## 注意事項

- `PostEffectRenderTargets::initialize()` は `DX12::initialize()` **後** に呼ぶ
- 同じ型のエフェクトは1つの `PostEffectComponent` に1つしか登録できない（重複時は既存インスタンスが返る）
- `PostEffectComponent` は `IRenderComponent` を継承しておらず、`RenderManager` の描画スレッドには乗らない。シーンの `draw()` から明示的に `execute()` を呼ぶ
- **全 `.cpp` ファイルの先頭に `#include "pch.h"` を記述する（プロジェクト規約）**
- 複数パスエフェクトで中間 RT を作成する場合、SRV スロットは `DescriptorHeapManager::Instance().allocateRange()` で確保する
- `ShaderData.h` の `shaderTable` 要素数は `ShaderID::MAX` と **必ず一致させる**（ずれるとクラッシュ）
- 新しい `RootSignatureType` を追加した場合は `RootSignatureManager::initialize()` から `buildXxx()` を呼ぶのを忘れない