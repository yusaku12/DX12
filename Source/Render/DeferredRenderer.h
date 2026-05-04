#pragma once

#include "Graphics/ConstantBuffer.h"

//=====================================================
//! Deferred ライティング
//=====================================================
class DeferredRenderer
{
public:

    static DeferredRenderer& Instance()
    {
        static DeferredRenderer instance;
        return instance;
    }

    //! 初期化
    void initialize();

    //! リサイズ対応
    void resize(UINT width, UINT height);

    //! ライティングパスのみ
    void renderLighting();

private:

    DeferredRenderer() = default;

    struct LightParams
    {
        Vector3 direction = Vector3(0.0f, -1.0f, 0.0f);
        float intensity = 3.0f;
        Vector3 color = Vector3(1.0f, 1.0f, 1.0f);
        float padding = 0.0f;
    };

    std::unique_ptr<ConstantBuffer<LightParams>> m_lightCB;
    LightParams m_lightParams{};
    size_t m_lightingPsoKey = 0;
};