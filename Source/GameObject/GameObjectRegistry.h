#pragma once

class GameObject;

//=====================================================
// GameObject の存在管理のみを行うレジストリ
//=====================================================
class GameObjectRegistry
{
public:

    //! シングルトンインスタンス取得
    static GameObjectRegistry& Instance()
    {
        static GameObjectRegistry instance;
        return instance;
    }

    //! 登録
    void registryGameObject(GameObject* obj);

    //! 登録解除
    void unregister(GameObject* obj);

    //! 登録されたゲームオブジェクトを更新
    void update();

    //! 全ての登録オブジェクト取得
    const std::vector<GameObject*>& getAll() const { return m_objects; }

private:

    std::vector<GameObject*> m_objects;
};