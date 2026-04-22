#pragma once

#include "Component.h"
#include "PostEffect\PostEffectBase.h"

//=====================================================
//! ポストエフェクトコンポーネント
//! Unity の Volume に相当
//! Component を直接継承（RenderManager のマルチスレッド描画には乗らない）
//=====================================================
class PostEffectComponent : public Component
{
public:

    ~PostEffectComponent() override = default;

    //! 初期化
    void awake() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! エフェクトを追加
    template<typename T, typename... Args>
    T* addEffect(Args&&... args)
    {
        static_assert(std::is_base_of_v<PostEffectBase, T>, "T must derive from PostEffectBase");
        //! 同じ型の重複チェック
        if (getEffect<T>())
        {
            LOG_WARN("PostEffect already exists, skipping add");
            return getEffect<T>();
        }
        auto effect = std::make_unique<T>(std::forward<Args>(args)...);
        effect->initialize();
        T* ptr = effect.get();
        m_effects.push_back(std::move(effect));
        sortEffects();
        return ptr;
    }

    //! エフェクトを取得
    template<typename T>
    T* getEffect() const
    {
        for (auto& e : m_effects)
        {
            if (auto* casted = dynamic_cast<T*>(e.get()))
                return casted;
        }
        return nullptr;
    }

    //! エフェクトを削除
    template<typename T>
    void removeEffect()
    {
        std::erase_if(m_effects,
            [](const std::unique_ptr<PostEffectBase>& e)
            {
                return dynamic_cast<T*>(e.get()) != nullptr;
            });
    }

    //! ポストエフェクトチェーンを実行
    //! @param sceneSrvIndex シーン RT の SRV インデックス
    //! @return 最終出力の SRV インデックス
    UINT execute(UINT sceneSrvIndex);

    //! 有効なエフェクトが存在するか
    bool hasActiveEffects() const;

private:

    //! 優先度順にソート
    void sortEffects();

    std::vector<std::unique_ptr<PostEffectBase>> m_effects;
};