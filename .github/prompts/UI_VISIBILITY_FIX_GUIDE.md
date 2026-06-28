# UI 非表示問題 — 完全な診断と修正ガイド

## 🔴 UI が見えない主な原因

### 1️⃣ **Canvas が作成されていない**（最も一般的）
- **症状**: シーンに UI コンポーネントを追加したが何も見えない
- **原因**: UIRenderer は Canvas を通してのみ動作
- **修正**: Canvas GameObject を作成し、CanvasComponent を addComponent する

```cpp
// ❌ 間違い（Canvas なし）
auto* button = new GameObject("MyButton");
button->addComponent<UIButtonComponent>();

// ✅ 正解（Canvas が必須）
auto* canvas = new GameObject("Canvas");
canvas->addComponent<CanvasComponent>();

auto* button = new GameObject("MyButton", canvas);  // canvas を親として指定
button->addComponent<RectTransformComponent>();
button->addComponent<UIButtonComponent>();
```

---

### 2️⃣ **RectTransformComponent が追加されていない**
- **症状**: Canvas は作成されているがUIが何も描画されない
- **原因**: RuntimeUIManager::drawNativeRecursive() は RectTransformComponent で位置・サイズを計算
- **修正**: すべての UI ウィジェットに RectTransformComponent を追加

```cpp
// ❌ 間違い
auto* button = new GameObject("Button", canvas);
button->addComponent<UIButtonComponent>();  // RectTransform なし

// ✅ 正解
auto* button = new GameObject("Button", canvas);
button->addComponent<RectTransformComponent>();  // ← 必須
button->addComponent<UIButtonComponent>();
```

---

### 3️⃣ **Canvas が enabled 状態でない**
- **症状**: Canvas は作成したが onEnable() が呼ばれていない
- **原因**: onEnable() で registerCanvas() が呼ばれる
  - GameObject は デフォルトで enabled
  - setEnabled(false) すると onEnable() が呼ばれない
- **修正**: Canvas が enabled 状態であることを確認

```cpp
auto* canvas = new GameObject("Canvas");
canvas->addComponent<CanvasComponent>();
// canvas->setEnabled(false);  // ← これをやるとUI描画されない

// ちゃんと enabled 状態を確認
assert(canvas->isEnabled());  // true のはず
```

---

### 4️⃣ **RectTransform のサイズが 0 の場合**
- **症状**: Canvas は登録されたが何も見えない
- **原因**: RuntimeUIManager::drawNativeRecursive() は resolveNativeRect() で矩形を計算
  - サイズが 0 の場合は描画対象外になる可能性
- **修正**: RectTransformComponent のサイズを設定

```cpp
auto* rect = button->addComponent<RectTransformComponent>();
rect->setPosition(Vector2(100, 100));
rect->setSize(Vector2(200, 50));  // ← サイズ 0 だと描画されない
```

---

## ✅ **正常な UI セットアップの流れ**

```cpp
// Scene::onEnter() 内で実行

// ★ステップ 1: Canvas 作成
GameObject* canvas = new GameObject("UICanvas");
auto* canvasComp = canvas->addComponent<CanvasComponent>();
canvasComp->setRenderMode(CanvasRenderMode::ScreenOverlay);
canvasComp->setSortOrder(0);

// Canvas は自動的に enabled になる
// → onEnable() が呼ばれる
// → registerCanvas() が実行される
// → RuntimeUIManager::m_canvases に登録される

// ★ステップ 2: UI ウィジェット作成（Canvas を親として）
GameObject* button = new GameObject("MyButton", canvas);  // ← canvas が親

// ★ステップ 3: RectTransform を追加（位置・サイズ情報）
auto* rect = button->addComponent<RectTransformComponent>();
rect->setPosition(Vector2(100, 100));
rect->setSize(Vector2(200, 50));

// ★ステップ 4: UI コンポーネント追加
auto* btn = button->addComponent<UIButtonComponent>();
btn->setLabel("Click Me");

// これ以後、RuntimeUIManager::renderNative() により毎フレーム描画される
```

---

## 🔍 **診断チェックリスト**

UI が見えない場合、以下を**ローカルブレーク**して確認してください：

```cpp
// RuntimeUIManager.cpp::renderNative() 相当の診断
void diagnoseUI()
{
    // ① Canvas が存在するか？
    size_t canvasCount = RuntimeUIManager::Instance().m_canvases.size();
    LOG_INFO("Registered canvases: %zu", canvasCount);
    if (canvasCount == 0)
    {
        LOG_ERROR("❌ Canvas が登録されていません");
        return;
    }

    // ② UIRenderer が初期化されているか？
    bool rendererInit = UIRenderer::Instance().isInitialized();
    LOG_INFO("UIRenderer initialized: %s", rendererInit ? "true" : "false");
    if (!rendererInit)
    {
        LOG_ERROR("❌ UIRenderer が初期化されていません");
        return;
    }

    // ③ Canvas が enabled 状態か？
    for (const auto* canvas : RuntimeUIManager::Instance().m_canvases)
    {
        LOG_INFO("Canvas '%s': enabled=%s, inHierarchy=%s",
            canvas->gameObject()->getName().c_str(),
            canvas->isActiveInHierarchy() ? "true" : "false",
            canvas->gameObject()->isEnabled() ? "true" : "false");
    }

    // ④ Canvas の子に RectTransform があるか？
    // RuntimeUIManager::drawNativeRecursive() で確認される
    LOG_INFO("✅ UI 診断完了");
}
```

---

## 📋 **Scene 別の実装例**

### ParticleScene での実装（修正済み）
- [ParticleScene.cpp](../ParticleScene.cpp) を参照
- Canvas + Button + Text + Panel の組み合わせ

### テンプレート関数の使用
- [UISetupTemplate.h](../UISetupTemplate.h) 参照
- `UISetup::createScreenCanvas()`
- `UISetup::addButtonToCanvas()`
- `UISetup::addTextToCanvas()`
- `UISetup::addPanelToCanvas()`

---

## 🔗 **内部フロー図**

```
Scene::onEnter()
  ↓
Create Canvas GameObject
  ↓
addComponent<CanvasComponent>()
  ↓
Canvas.enabled = true (default)
  ↓
Component::onEnable() 呼ばれる
  ↓
CanvasComponent::onEnable()
  ↓
RuntimeUIManager::registerCanvas(this)
  ↓
m_canvases に追加される
  ↓
毎フレーム: Window::render()
  ↓
RuntimeUIManager::renderNative()
  ↓
m_canvases.empty() ? NO
UIRenderer::isInitialized() ? YES
  ↓
キャンバスをソート → drawNativeRecursive() で描画
  ↓
UIRenderer::end() → GPU描画コマンド発行
  ↓
画面に表示される ✅
```

---

## 🚀 **推奨される検証ステップ**

1. **ParticleScene の修正版を実行**して UI が表示されることを確認
2. **UISetupTemplate.h の関数を使う** → 正しいセットアップが自動化される
3. **別のシーンにコピー** → 同じパターンで UI を追加
4. **イベントリスニング追加** → クリック検出を確認
5. **複数 Canvas でテスト** → sortOrder による重ね順を確認

---

## 📌 **よくある質問**

**Q. Canvas を複数作成できる？**
- A. はい。sortOrder で描画順序を制御できます

**Q. ScreenOverlay と WorldSpace の違いは？**
- A. ScreenOverlay = 画面座標（2D固定）/ WorldSpace = ワールド座標（3D投影）

**Q. RectTransform のアンカーはどう使う？**
- A. anchor = 親内での相対位置（0-1）。デフォルト(0.5, 0.5) = 中央

**Q. UI クリック検出は？**
- A. RuntimeUIManager が自動的にマウス判定 → UIButtonComponent::invokeClick()
