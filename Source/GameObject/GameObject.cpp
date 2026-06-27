#include "pch.h"
#include "Editor/EditorTransaction.h"
#include "GameObject\GameObject.h"
#include "Component\TransformComponent.h"

GameObject::GameObject(const std::string& name)
{
    setName(name);

    // 登録
    GameObjectRegistry::Instance().registryGameObject(this);
}

GameObject::~GameObject()
{
    // 親から外す
    setParent(nullptr);

    // 子をルートに戻す
    for (auto* child : m_children)
    {
        child->m_parent = nullptr;
    }
    m_children.clear();

    // コンポーネント破棄
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
    m_started = true;

    for (auto& c : m_components)
    {
        if (c)
            c->ensureStarted();
    }
}

void GameObject::destroy()
{
    if (m_destroyed)
        return;

    m_destroyed = true;

    // 子も再帰的に削除予約
    for (auto* child : m_children)
    {
        child->destroy();
    }
}

void GameObject::update()
{
    // GameObject 自体が無効なら何もしない（所属コンポーネントの実行を停止する）
    if (!isEnabled()) return;

    for (auto& c : m_components)
    {
        c->onUpdate();
    }
}

void GameObject::lateUpdate()
{
    if (!isEnabled()) return;

    for (auto& c : m_components)
    {
        c->onLateUpdate();
    }
}

void GameObject::drawInspector()
{
    const uint64_t objectId = getInstanceId();
    static std::unordered_map<uint64_t, std::string> s_nameEditStart;

    std::array<char, 256> nameBuffer{};
    strncpy_s(nameBuffer.data(), nameBuffer.size(), m_name.c_str(), _TRUNCATE);
    if (ImGui::InputText("Name", nameBuffer.data(), nameBuffer.size()))
    {
        setName(nameBuffer.data());
    }

    if (ImGui::IsItemActivated())
    {
        s_nameEditStart[objectId] = m_name;
    }

    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        const auto it = s_nameEditStart.find(objectId);
        if (it != s_nameEditStart.end())
        {
            const std::string before = it->second;
            const std::string after = m_name;
            s_nameEditStart.erase(it);

            if (before != after)
            {
                EditorTransaction::Manager::Instance().record(
                    "Rename GameObject",
                    [objectId, before]()
                    {
                        GameObject* object = GameObjectRegistry::Instance().findByInstanceId(objectId);
                        if (object && !object->isDestroyed())
                        {
                            object->setName(before);
                        }
                    },
                    [objectId, after]()
                    {
                        GameObject* object = GameObjectRegistry::Instance().findByInstanceId(objectId);
                        if (object && !object->isDestroyed())
                        {
                            object->setName(after);
                        }
                    });
            }
        }
    }

    ImGui::SameLine();

    // GameObject の有効/無効
    bool enabled = isEnabled();
    if (ImGui::Checkbox("Enabled", &enabled))
    {
        const bool previousEnabled = m_enabled;
        setEnabled(enabled);

        const bool nextEnabled = m_enabled;
        if (previousEnabled != nextEnabled)
        {
            EditorTransaction::Manager::Instance().record(
                "Toggle GameObject Enabled",
                [objectId, previousEnabled]()
                {
                    GameObject* object = GameObjectRegistry::Instance().findByInstanceId(objectId);
                    if (object && !object->isDestroyed())
                    {
                        object->setEnabled(previousEnabled);
                    }
                },
                [objectId, nextEnabled]()
                {
                    GameObject* object = GameObjectRegistry::Instance().findByInstanceId(objectId);
                    if (object && !object->isDestroyed())
                    {
                        object->setEnabled(nextEnabled);
                    }
                });
        }
    }

    ImGui::Separator();

    // 親表示（読み取り専用）
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

    // TransformComponent のギズモ更新は Inspector の開閉に依存せず毎フレーム行う
    if (auto tf = getComponent<TransformComponent>())
    {
        tf->onGizmo();
    }
}

void GameObject::setParent(GameObject* parent)
{
    if (m_parent == parent)
        return;

    // 旧親から外す
    if (m_parent)
    {
        auto& siblings = m_parent->m_children;
        siblings.erase(
            std::remove(siblings.begin(), siblings.end(), this),
            siblings.end()
        );
    }

    m_parent = parent;

    // 新親に追加
    if (m_parent)
    {
        m_parent->m_children.push_back(this);
    }

    // 親が変わるとワールド行列に依存する子孫の Transform が変化するため再計算フラグを立てる
    std::function<void(GameObject*)> markDirtyRec = [&](GameObject* node)
        {
            if (!node) return;
            if (auto tf = node->getComponent<TransformComponent>())
            {
                tf->markDirty();
            }
            for (auto* child : node->m_children)
                markDirtyRec(child);
        };

    markDirtyRec(this);
}

void GameObject::setEnabled(bool value)
{
    if (m_enabled == value) return;
    m_enabled = value;

    // GameObject の有効/無効が変わったとき、所属コンポーネントのうち
    // コンポーネント自身が有効なものに対して onEnable/onDisable を発行する。
    for (auto& c : m_components)
    {
        if (!c) continue;
        if (c->isEnabled())
        {
            if (m_enabled)
                c->onEnable();
            else
                c->onDisable();
        }
    }
}