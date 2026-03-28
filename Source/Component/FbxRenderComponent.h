#pragma once

#include "Model\FBXLoad.h"
#include "Model\Model.h"
#include "IRenderComponent.h"
#include "Graphics\ConstantBuffer.h"

class TransformComponent;

#define MAX_BONES 256

//============================================================================
// FbxRenderComponent — FBX レンダラーコンポーネント
//============================================================================
class FbxRenderComponent : public IRenderComponent
{
public:

    explicit FbxRenderComponent(const std::string& fbxPath);
    ~FbxRenderComponent() override = default;

    //! 初期化
    void awake() override;

    //! 更新処理
    void update() override;

    //! 描画（シングルスレッド描画）
    void render() override;

    //! 描画（マルチスレッド描画）
    void render(ID3D12GraphicsCommandList* cmd) override;

private:

    //! デバッグ描画モード
    enum class DebugMode : int
    {
        None = 0,          //!< 通常描画
        Wireframe,         //!< ワイヤーフレーム
        Max
    };

    //! モデル行列 CBV
    struct ModelCB
    {
        Matrix boneTransforms[MAX_BONES] = {};
    };

    //! マテリアル CBV 構造体
    struct MaterialCB
    {
        Vector4 diffuse = {};
    };

    //! FBX ファイルを読み込む
    bool loadFbx(const std::string& fbxPath);

    //! GPU リソースを構築
    void buildGPUResources();

    //! モデル行列 CBV 構築
    void createMaterialCBV();

    //! ソリッド描画用 PSO 構築
    void createSolidPSO();

    //! ワイヤーフレーム描画用 PSO 構築
    void createWireframePSO();

    //! 描画コア処理（PSO 別）
    void renderInternal(ID3D12GraphicsCommandList* cmd, size_t psoKey);

    std::unique_ptr<ConstantBuffer<ModelCB>> m_modelCB;
    std::unique_ptr<ConstantBuffer<MaterialCB>> m_materialCB;
    std::unique_ptr<Model> m_model;
    size_t m_solidPSOKey = 0;
    size_t m_wireframePSOKey = 0;
    DebugMode   m_debugMode = DebugMode::Wireframe;
    TransformComponent* m_transform = nullptr;
};