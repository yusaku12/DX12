#pragma once

#include "Component\Component.h"
#include "Component\Object.h"

//=====================================================
//! GameObject に付与できるタグ
//=====================================================
enum class Tag
{
    PostEffect,
    MAX
};

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
        std::string typeName = typeid(T).name();
        const std::string classPrefix = "class ";
        const std::string structPrefix = "struct ";
        if (typeName.rfind(classPrefix, 0) == 0)
            typeName = typeName.substr(classPrefix.size());
        else if (typeName.rfind(structPrefix, 0) == 0)
            typeName = typeName.substr(structPrefix.size());
        comp->setName(typeName);

        T* ptr = comp.get();
        m_componentMap[typeid(T)] = ptr;
        m_components.push_back(std::move(comp));

        ptr->awake();

        //! すでに start 済みの場合、条件が揃っていれば start を 1 回だけ実行する
        if (m_started) ptr->ensureStarted();
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

    //! GameObject の有効・無効設定
    //! 有効/無効が切り替わると、所属コンポーネントのうちコンポーネント自身が有効なものに対して
    //! onEnable/onDisable が発行される（Component::m_enabled が true のものだけ）。
    void setEnabled(bool value);

    //! 削除予約されているか
    bool isDestroyed() const { return m_destroyed; }

    //! GameObject 自体の有効・無効（GameObject が無効の場合、所属コンポーネントは実行されない）
    bool isEnabled() const { return m_enabled; }

    //! GameObject が start 済みか
    bool isStarted() const { return m_started; }

    //! 親オブジェクトの取得
    GameObject* getParent() const { return m_parent; }

    //! 子オブジェクト一覧の取得
    const std::vector<GameObject*>& getChildren() const { return m_children; }

    //! 所有しているコンポーネント一覧の取得（シリアライズ用）
    const std::vector<std::unique_ptr<Component>>& getComponents() const { return m_components; }

    //! タグを追加する
    void addTag(Tag tag) { m_tags.insert(tag); }

    //! タグを削除する
    void removeTag(Tag tag) { m_tags.erase(tag); }

    //! 指定したタグを持っているか
    bool hasTag(Tag tag) const { return m_tags.contains(tag); }

    //! 全タグを削除する
    void clearTags() { m_tags.clear(); }

    //! タグ一覧の取得
    const std::unordered_set<Tag>& getTags() const { return m_tags; }

    //! Prefabアセットパスの設定/取得
    void setPrefabAssetPath(const std::string& path) { m_prefabAssetPath = path; }
    const std::string& getPrefabAssetPath() const { return m_prefabAssetPath; }
    void clearPrefabAssetPath() { m_prefabAssetPath.clear(); }
    bool isPrefabInstanceRoot() const { return !m_prefabAssetPath.empty(); }

private:

    bool m_started = false;
    bool m_destroyed = false;   //!< 削除予約フラグ
    bool m_enabled = true;      //!< GameObject の有効フラグ（デフォルト有効）
    std::vector<std::unique_ptr<Component>> m_components; //!< 実体保持
    std::unordered_map<std::type_index, Component*> m_componentMap; //!< 高速検索用
    GameObject* m_parent = nullptr;
    std::vector<GameObject*> m_children;
    std::unordered_set<Tag> m_tags; //!< タグ一覧
    std::string m_prefabAssetPath; //!< Prefabアセット参照（インスタンスルートのみ保持）
};