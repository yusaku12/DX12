#pragma once

#include "Behaviour.h"

//=====================================================
// ユーザーが継承するスクリプト基底
// MonoBehaviour 相当
//=====================================================
class ScriptBehaviour : public Behaviour
{
public:

    virtual ~ScriptBehaviour() = default;

    //! インスペクタ表示用
    void onInspectorGUI() override
    {
        Behaviour::onInspectorGUI();
        ImGui::TextDisabled("Script: %s", getName().c_str());
    }

protected:

    //! 衝突したときに呼ばれる
    virtual void onCollisionEnter() {}

    //! トリガーに入ったときに呼ばれる
    virtual void onTriggerEnter() {}
};