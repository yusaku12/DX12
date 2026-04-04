#pragma once

class GameObject;

//=====================================================
// ObjectPicker
// - Scene ウィンドウ上でマウスクリックしたオブジェクトを選択する
//=====================================================
class ObjectPicker
{
public:

    //! シングルトンインスタンス取得
    static ObjectPicker& Instance()
    {
        static ObjectPicker instance;
        return instance;
    }

    //! 毎フレームの更新処理（マウスクリック検出 → レイキャスト → 選択更新）
    void update();

    //! ピッキングの有効・無効（ImGuizmo 操作中は無効にするなど）
    void setEnabled(bool enable) { m_enabled = enable; }
    bool isEnabled() const { return m_enabled; }

private:

    ObjectPicker() = default;
    ~ObjectPicker() = default;
    ObjectPicker(const ObjectPicker&) = delete;
    ObjectPicker& operator=(const ObjectPicker&) = delete;

    //! クリック座標が Scene ウィンドウ内ならレンダリングビューポート座標を返す
    bool getViewportCoord(float& outVpX, float& outVpY) const;

    //! 全 GameObject の AABB に対してレイ交差判定を行い、最も近いオブジェクトを返す
    GameObject* pickByAABB(const Ray& ray, float maxDistance) const;

    bool m_enabled = true;
};