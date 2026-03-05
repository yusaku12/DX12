#pragma once

//============================================================
// Scene 基底クラス
//============================================================
class Scene
{
public:

    virtual ~Scene() = default;

    //! シーン生成時に1回だけ呼ばれる
    virtual void onEnter() {}

    //! シーン破棄時に1回だけ呼ばれる
    virtual void onExit() {}

    //! 毎フレーム更新
    virtual void update() = 0;

    //! 描画
    virtual void draw() = 0;

    //! マルチスレッド描画（ワーカーコマンドリストにコマンドを記録）
    //! デフォルト実装は draw() を呼ぶだけ（後方互換）
    virtual void drawMultiThreaded() { draw(); }

    //! シーン毎のデバック描画
    virtual void debugDraw() = 0;
};
