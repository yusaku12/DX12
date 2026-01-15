#pragma once

#include "Component.h"
#include <imgui.h>

//=====================================================
// 実行系 Component
// enabled が false の場合 Update されない
//=====================================================
class Behaviour : public Component
{
public:

    //! 毎フレーム呼ばれる
    void update() final
    {
        if (!m_enabled) return;
        onUpdate();
    }

    //! 毎フレーム呼ばれる（update の後）
    void lateUpdate() final
    {
        if (!m_enabled) return;
        onLateUpdate();
    }

    //! インスペクタ表示用
    void onInspectorGUI() override
    {
        bool enabled = isEnabled();
        if (ImGui::Checkbox("Enabled", &enabled))
        {
            setEnabled(enabled);
        }
    }

protected:

    //! 毎フレーム呼ばれる
    virtual void onUpdate() {}

    //! 毎フレーム呼ばれる（update の後）
    virtual void onLateUpdate() {}
};