#pragma once

#include "Model\FBXLoad.h"
#include "IRenderComponent.h"
#include "Graphics\ConstantBuffer.h"

#include <filesystem>

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

    //! GBuffer 描画
    void renderGBuffer(ID3D12GraphicsCommandList* cmd) override;

    //! シャドウ深度描画
    void renderShadowDepth(ID3D12GraphicsCommandList* cmd) override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! モデルのワールド空間 AABB を取得する（ピッキング・カリング用）
    bool getWorldAABB(Vector3& outCenter, Vector3& outExtents) const override;

    //! 自動LOD評価
    int evaluateAutoLodLevel(const Vector3& cameraPosition) const override;

    //! ランタイムLOD適用
    void setRuntimeLodLevel(int lodLevel) override;

    //! LOD段数
    int getLodLevelCount() const override;

    //! モデル取得
    Model* getModel() const { return m_model.get(); }

    //! モデルパス取得（シリアライズ用）
    const std::string& getModelPath() const { return m_modelPath; }

private:

    //! デバッグ描画モード
    enum class DebugMode : int
    {
        None = 0,          //!< 通常描画
        Wireframe,         //!< ワイヤーフレーム
    };

    //! モデル行列 CBV
    struct ModelCB
    {
        Matrix boneTransforms[MAX_BONES] = {};
        Vector4 objectMotion = Vector4::Zero;
    };

    //! マテリアル CBV 構造体
    struct MaterialCB
    {
        Vector4 diffuse = {};
        Vector3 pbr = { 1.0f, 1.0f, 1.0f };
        float graphId = 0.0f;
    };

    //! モデルアセットを読み込む
    bool loadModelAsset(const std::string& modelPath);

    //! 自動検出された LOD モデルを読み込む
    void loadAutoLodAssets();

    //! ベースメッシュから自動 LOD を生成する
    void generateAutoLodAssets();

    //! 指定比率で簡易デシメーションした LOD モデルを構築
    static bool buildReducedLodModel(const ModelResource::Model& src, float triangleRatio, bool mergeByMaterial, ModelResource::Model& out);

    //! メッシュ境界を再計算
    static void rebuildMeshBounds(ModelResource::Mesh& mesh);

    //! GPU リソースを構築
    void buildGPUResources();

    //! モデル行列 CBV 構築
    void createMaterialCBV();

    //! 共通の入力レイアウト定義を取得
    static std::vector<D3D12_INPUT_ELEMENT_DESC> getInputLayout();

    //! PSO を構築（RasterizerState を指定）
    size_t createPSO(RasterizerState rasterizer);

    //! GBuffer PSO を構築
    size_t createGBufferPSO();

    //! シャドウ深度 PSO を構築
    size_t createShadowDepthPSO();

    //! 描画コア処理（PSO 別）
    void renderInternal(ID3D12GraphicsCommandList* cmd, size_t psoKey);

    //! AABB 描画
    void renderAABB();

    //! ImGui：統計パネル
    void imguiStatisticsPanel();

    //! ImGui：メッシュ情報パネル
    void imguiMeshPanel();

    //! ImGui：マテリアル情報パネル
    void imguiMaterialPanel();

    //! ImGui：ボーン情報パネル
    void imguiBonePanel();

    //! ImGui：ボーンツリー再帰描画
    void imguiBoneTreeNode(int boneIndex, const std::vector<ModelResource::Bone>& bones, const std::vector<std::vector<int>>& childMap);

    //! ImGui：デバッグ描画パネル
    void imguiDebugPanel();

    //! ImGui：エクスポートパネル
    void imguiExportPanel();

    //! マテリアル CBV を再更新（色変更時）
    void updateMaterialCBV();

    //! マテリアル名ベースのグラフIDプリセットを適用
    void applyMaterialGraphBindings();

    struct LodEntry
    {
        std::unique_ptr<Model> model;
        std::unique_ptr<ConstantBuffer<ModelCB>> modelCB;
        std::unique_ptr<ConstantBuffer<MaterialCB>> materialCB;
        std::string sourcePath;
    };

    //! 現在有効な LOD エントリを取得
    LodEntry* getActiveLodEntry();
    const LodEntry* getActiveLodEntry() const;

    //! テキスト末尾が _LODn / _lodn かを判定
    static bool isLodSuffix(const std::string& stem, size_t& suffixPos, int& lodIndex);

    //! 指定パスから探索用ベース名を作る
    static std::string buildLodBaseStem(const std::filesystem::path& modelPath);

    std::unique_ptr<ConstantBuffer<ModelCB>> m_modelCB;
    std::unique_ptr<ConstantBuffer<MaterialCB>> m_materialCB;
    size_t m_solidPSOKey = 0;
    size_t m_wireframePSOKey = 0;
    size_t m_gbufferPSOKey = 0;
    size_t m_shadowPSOKey = 0;
    DebugMode m_debugMode = DebugMode::None;
    TransformComponent* m_transform = nullptr;
    std::unique_ptr<Model> m_model;
    std::string m_modelPath;

    std::vector<LodEntry> m_lods;
    int m_runtimeLod = 0;
    bool m_enableAutoLod = true;
    std::vector<float> m_lodSwitchDistances = { 20.0f, 45.0f, 85.0f };
    bool m_enableRuntimeLodGeneration = true;
    bool m_enableRuntimeLodMerge = true;
    std::vector<float> m_generatedLodRatios = { 0.50f, 0.25f, 0.12f };
    std::vector<int> m_materialGraphIds;
    Vector3 m_prevPosition = Vector3::Zero;
    Vector3 m_frameMotion = Vector3::Zero;
    bool m_hasPrevPosition = false;
};