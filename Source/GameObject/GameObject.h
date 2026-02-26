#pragma once

#include "Component\Component.h"
#include "Component\Object.h"

//=====================================================
// Component を束ねる箱
// UnityEngine.GameObject 相当
//=====================================================
class GameObject : public Object
{
public:

    explicit GameObject(const std::string& name = "GameObject");
    ~GameObject();

    //! コンポーネント追加
    template<class T, class... Args>
    T* addComponent(Args&&... args)
    {
        static_assert(std::is_base_of_v<Component, T>);

        //! 同一型のコンポーネントが既に存在する場合は既存を返す（現在の実装は型1つにつき1コンポーネント扱い）
        auto it = m_componentMap.find(typeid(T));
        if (it != m_componentMap.end())
        {
            return static_cast<T*>(it->second);
        }

        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        comp->m_gameObject = this;
        comp->setName(typeid(T).name());

        T* ptr = comp.get();
        m_componentMap[typeid(T)] = ptr;
        m_components.push_back(std::move(comp));

        ptr->awake();
        if (m_started) ptr->start();
        return ptr;
    }

    //! コンポーネント取得
    template<class T>
    T* getComponent() const
    {
        auto it = m_componentMap.find(typeid(T));
        return it == m_componentMap.end()
            ? nullptr
            : static_cast<T*>(it->second);
    }

    //! ゲーム開始時に一度だけ呼ばれる
    void start();

    //! 削除予約
    void destroy();

    //! 毎フレーム呼ばれる
    void update();

    //! 毎フレーム呼ばれる（update の後）
    void lateUpdate();

    //! インスペクタ表示用
    void drawInspector();

    //! 親子関係の設定
    void setParent(GameObject* parent);

    //! 親オブジェクトの取得
    GameObject* getParent() const { return m_parent; }

    //! 子オブジェクト一覧の取得
    const std::vector<GameObject*>& getChildren() const { return m_children; }

    //! 削除予約されているか
    bool isDestroyed() const { return m_destroyed; }

private:

    bool m_started = false;
    bool m_destroyed = false;   //!< 削除予約フラグ
    std::vector<std::unique_ptr<Component>> m_components; //!< 実体保持
    std::unordered_map<std::type_index, Component*> m_componentMap; //!< 高速検索用
    GameObject* m_parent = nullptr;
    std::vector<GameObject*> m_children;
};