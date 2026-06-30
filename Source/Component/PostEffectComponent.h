#pragma once

#include "Component.h"
#include "PostEffect\PostEffectBase.h"

class TransformComponent;

//=====================================================
//! ポストエフェクトコンポーネント
//! Unity の Volume に相当
//! Component を直接継承（RenderManager のマルチスレッド描画には乗らない）
//=====================================================
class PostEffectComponent : public Component
{
public:

    //! デバッグ統計
    struct DebugStats
    {
        struct EffectEntry
        {
            std::string name;              //!< エフェクト名
            UINT inputSrvIndex = UINT_MAX; //!< 入力 SRV
            UINT outputSrvIndex = UINT_MAX;//!< 出力 SRV
            bool enabled = false;          //!< 有効状態
            bool executed = false;         //!< 実行されたか
        };

        int   executedEffects = 0;     //!< 実行されたエフェクト数
        float lastVolumeWeight = 0.0f; //!< 最後に使われたボリュームウェイト
        UINT  lastInputSrvIndex = 0;   //!< 最後の入力 SRV
        UINT  lastOutputSrvIndex = 0;  //!< 最後の出力 SRV
        bool  executed = false;        //!< 実行されたか

        std::vector<EffectEntry> effects; //!< エフェクトごとの出力
    };

    ~PostEffectComponent() override = default;

    //! 初期化
    void awake() override;

    //! 有効化
    void onEnable() override;

    //! 無効化
    void onDisable() override;

    //! 破棄
    void onDestroy() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! エフェクトを追加
    template<typename T, typename... Args>
    T* addEffect(Args&&... args)
    {
        static_assert(std::is_base_of_v<PostEffectBase, T>, "T must derive from PostEffectBase");
        //! 同じ型の重複チェック
        if (auto* existing = getEffect<T>())
        {
            LOG_WARN("PostEffect already exists, skipping add");
            return existing;
        }
        auto effect = DXMem::makeUnique<T>(std::forward<Args>(args)...);
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
    UINT execute(UINT sceneSrvIndex);

    //! Volume チェーンを実行（PostEffectManager から呼ぶ）
    bool executeChain(float volumeWeight);

    //! 有効なエフェクトが存在するか
    bool hasActiveEffects() const;

    //! 深度テクスチャが必要か
    bool requiresDepth() const;

    //! Volume 優先度
    int getVolumePriority() const { return m_volumePriority; }
    void setVolumePriority(int priority) { m_volumePriority = priority; }

    //! 重み
    float getWeight() const { return m_weight; }
    void setWeight(float weight);

    //! ブレンド距離
    float getBlendDistance() const { return m_blendDistance; }
    void setBlendDistance(float distance);

    //! グローバル Volume
    bool isGlobal() const { return m_isGlobal; }
    void setGlobal(bool value) { m_isGlobal = value; }

    //! カメラ位置に対するブレンドウェイト計算
    float computeBlendWeight(const Vector3& cameraPos) const;

    //! デバッグ統計取得
    const DebugStats& getDebugStats() const { return m_debug; }

    //! エフェクト一覧取得（シリアライズ用）
    const std::vector<std::unique_ptr<PostEffectBase>>& getEffects() const { return m_effects; }

private:

    //! 優先度順にソート
    void sortEffects();

    //! Manager 登録
    void registerToManager();

    //! Manager 解除
    void unregisterFromManager();

    std::vector<std::unique_ptr<PostEffectBase>> m_effects;

    int m_volumePriority = 0;
    float m_weight = 1.0f;
    float m_blendDistance = 0.0f;
    bool m_isGlobal = true;

    TransformComponent* m_transform = nullptr;
    bool m_registered = false;

    DebugStats m_debug;
};