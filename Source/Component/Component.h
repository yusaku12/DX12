#pragma once

#include "Object.h"

class GameObject;

//=====================================================
// GameObject に付与される機能単位
// UnityEngine.Component 相当
//=====================================================
class Component : public Object
{
public:

    virtual ~Component() = default;

    //! 生成直後に一度だけ呼ばれる
    virtual void awake() {}

    //! ゲーム開始時に一度だけ呼ばれる
    virtual void start() {}

    //! 毎フレーム呼ばれる
    virtual void update() {}

    //! 毎フレーム呼ばれる（update の後）
    virtual void lateUpdate() {}

    //! 有効化されたときに呼ばれる
    virtual void onEnable() {}

    //! 無効化されたときに呼ばれる
    virtual void onDisable() {}

    //! 破棄される直前に呼ばれる
    virtual void onDestroy() {}

    //! インスペクタ表示用
    virtual void onInspectorGUI() {}

    //! 所属している GameObject
    GameObject* gameObject() const { return m_gameObject; }

    //! 有効・無効の取得・設定
    bool isEnabled() const { return m_enabled; }

    //! 有効・無効の設定
    void setEnabled(bool value)
    {
        if (m_enabled == value) return;
        m_enabled = value;
        value ? onEnable() : onDisable();
    }

protected:

    friend class GameObject;
    GameObject* m_gameObject = nullptr; //!< 所属先
    bool m_enabled = true;
};