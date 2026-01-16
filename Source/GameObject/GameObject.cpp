#include "pch.h"
#include "GameObject\GameObject.h"

GameObject::GameObject(const std::string& name)
{
    setName(name);

    //! 登録
    GameObjectRegistry::Instance().registryGameObject(this);
}

GameObject::~GameObject()
{
    //! 親から外す
    setParent(nullptr);

    //! 子をルートに戻す
    for (auto* child : m_children)
    {
        child->m_parent = nullptr;
    }
    m_children.clear();

    //! コンポーネント破棄
    for (auto& c : m_components)
    {
        if (c)
        {
            c->onDestroy();
        }
    }
}

void GameObject::start()
{
    for (auto& c : m_components)
        c->start();
    m_started = true;
}

void GameObject::destroy()
{
    if (m_destroyed)
        return;

    m_destroyed = true;

    //! 子も再帰的に削除予約
    for (auto* child : m_children)
    {
        child->destroy();
    }
}

void GameObject::update()
{
    for (auto& c : m_components)
    {
        c->update();
    }
}

void GameObject::lateUpdate()
{
    for (auto& c : m_components)
    {
        c->lateUpdate();
    }
}

void GameObject::drawInspector()
{
    ImGui::InputText("Name", m_name.data(), m_name.capacity() + 1);
    ImGui::Separator();

    //! 親表示（読み取り専用）
    if (m_parent)
        ImGui::Text("Parent : %s", m_parent->getName().c_str());
    else
        ImGui::TextDisabled("Parent : None");

    ImGui::Separator();

    for (auto& comp : m_components)
    {
        ImGui::PushID(comp.get());

        if (ImGui::CollapsingHeader(
            comp->getName().c_str(),
            ImGuiTreeNodeFlags_DefaultOpen))
        {
            comp->onInspectorGUI();
        }

        ImGui::PopID();
        ImGui::Separator();
    }
}

void GameObject::setParent(GameObject* parent)
{
    if (m_parent == parent)
        return;

    //! 旧親から外す
    if (m_parent)
    {
        auto& siblings = m_parent->m_children;
        siblings.erase(
            std::remove(siblings.begin(), siblings.end(), this),
            siblings.end()
        );
    }

    m_parent = parent;

    //! 新親に追加
    if (m_parent)
    {
        m_parent->m_children.push_back(this);
    }
}