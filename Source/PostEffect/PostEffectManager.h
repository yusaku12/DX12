#pragma once

class PostEffectComponent;

//=====================================================
//! ポストエフェクト Volume 管理
//=====================================================
class PostEffectManager
{
public:

    static PostEffectManager& Instance()
    {
        static PostEffectManager instance;
        return instance;
    }

    //! Volume 登録
    void registerComponent(PostEffectComponent* comp);

    //! Volume 解除
    void unregisterComponent(PostEffectComponent* comp);

    //! 全 Volume を実行
    UINT execute(UINT sceneSrvIndex);

private:

    PostEffectManager() = default;

    std::vector<PostEffectComponent*> m_components;
    std::vector<PostEffectComponent*> m_sortedComponents;
    std::mutex m_mutex;
    bool m_dirty = false;
};