#pragma once

#include "IRenderComponent.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/ConstantBuffer.h"
#include "Graphics/LoadTexture.h"

//=====================================================
// SkyboxComponent
//=====================================================
class SkyboxComponent : public IRenderComponent
{
public:

    explicit SkyboxComponent(const std::wstring& cubemapPath = L"");
    ~SkyboxComponent() override = default;

    //! 初期化
    void awake() override;

    //! 描画(シングルスレッド)
    void render() override;

    //! 描画(マルチスレッド)
    void render(ID3D12GraphicsCommandList* cmd) override;

    //! Forward 描画
    void renderForward(ID3D12GraphicsCommandList* cmd) override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! キューブマップ設定
    void setCubemap(const std::wstring& path);

    //! 露光設定
    void setExposure(float value) { m_exposure = value; }

    //! 色調設定
    void setTint(const Vector3& tint) { m_tint = tint; }

    //! 回転設定（度）
    void setRotationDegrees(float value) { m_rotationDegrees = value; }

private:

    //! 頂点
    struct Vertex
    {
        Vector3 position = {};
    };

    //! Skybox パラメータ
    struct SkyboxParams
    {
        Vector3 tint = Vector3::One;
        float exposure = 1.0f;
        float rotation = 0.0f;
        Vector3 padding = Vector3::Zero;
    };

    //! メッシュ生成
    void buildMesh();

    //! PSO生成
    void buildPSO();

    //! パラメータ更新
    void updateParams();

    //! 描画コア
    void draw(ID3D12GraphicsCommandList* cmd);

    std::wstring m_cubemapPath;
    LoadTexture* m_cubemap = nullptr;
    std::unique_ptr<VertexBuffer<Vertex>> m_vertexBuffer;
    std::unique_ptr<IndexBuffer<uint16_t>> m_indexBuffer;
    std::unique_ptr<ConstantBuffer<SkyboxParams>> m_paramCB;
    size_t m_psoKey = 0;

    float m_exposure = 1.0f;
    float m_rotationDegrees = 0.0f;
    Vector3 m_tint = Vector3::One;
};