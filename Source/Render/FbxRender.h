#pragma once

#include "Model\ModelData.h"
#include "Model\FbxLoad.h"
#include "Graphics\VertexBuffer.h"
#include "Graphics\IndexBuffer.h"
#include "Graphics\ConstantBuffer.h"
#include "Component\IRenderComponent.h"

class TransformComponent;

//============================================================================
// FbxRender — モデルエディター向け FBX レンダラー
//
// 【設計方針】
//   1. FBX ファイルを読み込み → ModelData に変換 → GPU リソースを構築
//   2. ModelData を外部に公開し、エディター側で編集・保存を可能にする
//   3. 豊富なデバッグ機能（ワイヤーフレーム、法線、接線、AABB、UV チェッカー等）
//   4. IRenderComponent 実装により RenderManager に登録してマルチスレッド描画対応
//   5. メッシュ / マテリアル / サブセット単位の表示ON/OFF
//
// 【使い方】
//   FbxRender renderer("Assets/Model.fbx");
//   renderer.setTransform(&transformComponent);
//   renderer.render();                           // シングルスレッド
//   renderer.render(cmd);                        // マルチスレッド
//   renderer.debugImGui();                       // ImGui デバッグウィンドウ
//   renderer.getModelData().saveToMdl("out.mdl"); // .mdl 保存
//============================================================================
class FbxRender : public IRenderComponent
{
public:

    //! FBX ファイルパスからロード & GPU リソース構築
    explicit FbxRender(const std::string& fbxPath);

    //! 既に読み込み済みの FbxLoad::Model から構築
    explicit FbxRender(const FbxLoad::Model& fbxModel);

    ~FbxRender() override = default;

    //! コピー禁止
    FbxRender(const FbxRender&) = delete;
    FbxRender& operator=(const FbxRender&) = delete;

    //! 描画（メインコマンドリスト）
    void render() override;

    //! 描画（指定コマンドリスト — マルチスレッド用）
    void render(ID3D12GraphicsCommandList* cmd) override;

    //! デバッグ用名前
    const char* getRenderName() const override { return m_debugName.c_str(); }

    //! Transform 紐付け
    void setTransform(TransformComponent* tf) { m_transform = tf; }

    //! モデルデータ取得（読み取り専用）
    const ModelData& getModelData() const { return m_modelData; }

    //! モデルデータ取得（編集用）
    ModelData& getModelData() { return m_modelData; }

    //! .mdl として保存
    bool saveToMdl(const std::string& path) const { return m_modelData.saveToMdl(path); }

    //! デバッグ描画モード
    enum class DebugMode : int
    {
        None = 0,          //!< 通常描画
        Wireframe,         //!< ワイヤーフレーム
        WireframeOverlay,  //!< ソリッド + ワイヤーフレーム重畳
        Normals,           //!< 法線表示
        Tangents,          //!< 接線表示
        UVChecker,         //!< UV チェッカー（テクスチャなし・マテリアルカラーのみ）
        BoneWeights,       //!< ボーンウェイト可視化
        Max
    };

    //! ImGui デバッグウィンドウ（エディター統合用）
    void debugImGui();

    //! デバッグモード設定 / 取得
    void setDebugMode(DebugMode mode) { m_debugMode = mode; }
    DebugMode getDebugMode() const { return m_debugMode; }

    //! AABB 表示 ON / OFF
    void setShowBounds(bool show) { m_showBounds = show; }
    bool getShowBounds() const { return m_showBounds; }

    //! メッシュ単位の表示 ON / OFF
    void setMeshVisible(uint32_t meshIndex, bool visible);
    bool getMeshVisible(uint32_t meshIndex) const;

    //! マテリアル単位の表示 ON / OFF
    void setMaterialVisible(uint32_t materialIndex, bool visible);
    bool getMaterialVisible(uint32_t materialIndex) const;

    //! GPU リソースを再構築（ModelData 編集後に呼ぶ）
    void rebuild();

    //! ロード成功判定
    bool isValid() const { return m_valid; }

    //! 統計情報構造体
    struct Statistics
    {
        uint32_t totalVertices = 0;
        uint32_t totalIndices = 0;
        uint32_t totalTriangles = 0;
        uint32_t meshCount = 0;
        uint32_t materialCount = 0;
        uint32_t subMeshCount = 0;
        uint32_t drawCallCount = 0;   //!< 直近フレームのドローコール数
    };

    const Statistics& getStatistics() const { return m_stats; }

private:

    //! マテリアルあたりの最大テクスチャ数
    enum class TextureType : UINT
    {
        Diffuse,
        Normal,
        Max
    };

    //! マテリアル CBV 構造体
    struct MaterialCB
    {
        Vector4 diffuse;
        Vector3 specular;
        float   specularPower;
        Vector3 ambient;
        float   _pad0;
        Vector3 emissive;
        float   _pad1;
    };

    //! モデル行列 CBV
    struct ModelCB
    {
        Matrix world = {};
    };

    //! サブセット描画データ
    struct Subset
    {
        UINT indexCount = 0;
        UINT startIndex = 0;
        UINT materialIndex = 0;
        bool visible = true;
        std::array<int, static_cast<int>(TextureType::Max)> textureIndices{};
        UINT descriptorBase = UINT_MAX;
    };

    //! メッシュ描画データ
    struct MeshDrawData
    {
        std::unique_ptr<VertexBuffer<ModelVertex>> vertexBuffer;
        std::unique_ptr<IndexBuffer<uint32_t>>     indexBuffer;
        std::vector<Subset>                        subsets;
        bool visible = true;
    };

    //! ModelData → GPU リソース構築
    void buildGPUResources();

    //! マテリアル CBV 構築
    void createMaterialCBV();

    //! テクスチャ読み込み
    void createTextures();

    //! ソリッド描画用 PSO 構築
    void createSolidPSO();

    //! ワイヤーフレーム描画用 PSO 構築
    void createWireframePSO();

    //! Descriptor 再構築
    void rebuildSubsetDescriptors(Subset& subset);

    //! 統計情報を更新
    void computeStatistics();

    //! 描画コア処理（PSO 別）
    void renderInternal(ID3D12GraphicsCommandList* cmd, size_t psoKey);

    //! デバッグ描画：法線 / 接線の線分
    void debugDrawNormals(float length = 0.05f) const;
    void debugDrawTangents(float length = 0.05f) const;

    //! デバッグ描画：AABB
    void debugDrawBounds() const;

    //! ImGui：メッシュ情報パネル
    void imguiMeshPanel();

    //! ImGui：マテリアル情報パネル
    void imguiMaterialPanel();

    //! ImGui：統計パネル
    void imguiStatisticsPanel();

    //! ImGui：デバッグ描画パネル
    void imguiDebugPanel();

    //! ImGui：エクスポートパネル
    void imguiExportPanel();

    ModelData                                   m_modelData;
    std::vector<MeshDrawData>                   m_meshes;
    std::unique_ptr<ConstantBuffer<MaterialCB>> m_materialCB;
    std::unique_ptr<ConstantBuffer<ModelCB>>    m_modelCB;
    size_t                                      m_solidPSOKey = 0;
    size_t                                      m_wireframePSOKey = 0;
    std::vector<LoadTexture*>                   m_textures;
    std::vector<std::wstring>                   m_texturePaths;
    TransformComponent* m_transform = nullptr;

    static constexpr UINT TEXTURE_SLOT_COUNT = static_cast<UINT>(TextureType::Max);

    //! デバッグ
    DebugMode   m_debugMode = DebugMode::None;
    bool        m_showBounds = false;
    float       m_normalDisplayLength = 0.05f;
    float       m_tangentDisplayLength = 0.05f;
    Statistics  m_stats{};
    std::string m_debugName = "FbxRender";
    std::string m_sourcePath;   //!< 元の FBX ファイルパス

    bool m_valid = false;
};