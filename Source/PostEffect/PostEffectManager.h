#pragma once

class PostEffectComponent;
class ColorGradingEffect;

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
    UINT execute(UINT sceneSrvIndex, bool enableOptionalEffects = true);

private:

    PostEffectManager();
    ~PostEffectManager();

    void executeDisplayTransform();

    std::vector<PostEffectComponent*> m_components;
    std::vector<PostEffectComponent*> m_sortedComponents;
    std::mutex m_mutex;
    std::unique_ptr<ColorGradingEffect> m_displayTransform;
    bool m_dirty = false;
};