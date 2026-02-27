#pragma once

#include "Object.h"

class GameObject;

//=====================================================
// GameObject に付与される機能単位
// UnityEngine.Component
//=====================================================
class Component : public Object
{
public:

    virtual ~Component() = default;

    //! 派生クラスは初期化ロジックをここに実装する
    virtual void awake() {}

    //! ゲーム開始時に一度だけ呼ばれる
    virtual void start() {}

    //! 毎フレーム呼ばれる（派生クラスでオーバーライド）
    virtual void update() {}

    //! 毎フレーム呼ばれる（update の後、派生クラスでオーバーライド）
    virtual void lateUpdate() {}

    //! 有効化されたときに呼ばれる
    virtual void onEnable() {}

    //! 無効化されたときに呼ばれる
    virtual void onDisable() {}

    //! 破棄される直前に呼ばれる
    virtual void onDestroy() {}

    //! インスペクタ表示用（派生クラスでオーバーライド）
    virtual void inspectGUI() {};

    //! 有効でない場合は何もしない
    void onUpdate();

    //! 有効でない場合は何もしない
    void onLateUpdate();

    //! インスペクタ表示用（デフォルトで Enabled を表示）
    void onInspectorGUI();

    //! コンポーネントの有効・無効設定
    //! 所属 GameObject が無効な場合は onEnable の発行を遅延扱いにする。
    void setEnabled(bool value);

    //! コンポーネントが階層上で有効か（自分自身が有効 && 親 GameObject が有効）
    bool isActiveInHierarchy() const;

    //! コンポーネント自身の有効・無効の取得
    bool isEnabled() const { return m_enabled; }

    //! 所属している GameObject
    GameObject* gameObject() const { return m_gameObject; };

protected:

    friend class GameObject;
    GameObject* m_gameObject = nullptr; //!< 所属先
    bool m_enabled = true;
};