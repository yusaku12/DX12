#pragma once

#include "GameObject.h"

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

    //! 削除予約されたオブジェクトを破棄
    void destroyMarkedObjects();

    //! 終了処理（登録オブジェクトを破棄）
    void shutdown();

    //! 全ての登録オブジェクト取得
    const std::vector<GameObject*>& getAll() const { return m_objects; }

    //! 指定タグを持つ最初の GameObject を返す（見つからない場合は nullptr）
    GameObject* findByTag(Tag tag) const;

    //! 指定インスタンスIDの GameObject を返す（見つからない場合は nullptr）
    GameObject* findByInstanceId(uint64_t instanceId) const;

private:

    std::vector<GameObject*> m_objects;
};