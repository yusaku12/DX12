#include "pch.h"
#include "Component.h"
#include "GameObject\GameObject.h"

void Component::onUpdate()
{
    if (!isActiveInHierarchy()) return;

    //! 派生クラスの update を呼ぶ
    update();
}

void Component::onLateUpdate()
{
    if (!isActiveInHierarchy()) return;

    //! 派生クラスの lateUpdate を呼ぶ
    lateUpdate();
}

void Component::setEnabled(bool value)
{
    if (m_enabled == value) return;
    m_enabled = value;

    const bool gameActive = (m_gameObject == nullptr) ? true : m_gameObject->isEnabled();

    if (m_enabled && gameActive)
        onEnable();
    else
        onDisable();
}

bool Component::isActiveInHierarchy() const
{
    if (!m_enabled) return false;
    if (m_gameObject && !m_gameObject->isEnabled()) return false;
    return true;
}

void Component::onInspectorGUI()
{
    bool enabled = isEnabled();
    if (ImGui::Checkbox("Enabled", &enabled))
    {
        setEnabled(enabled);
    }

    //! 派生クラスのインスペクタ表示
    inspectGUI();
}