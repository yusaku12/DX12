#include "Model\FBXLoad.h"
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

    //! GBuffer 描画
    void renderGBuffer(ID3D12GraphicsCommandList* cmd) override;

    //! Forward 描画
    void renderForward(ID3D12GraphicsCommandList* cmd) override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! モデルのワールド空間 AABB を取得する（ピッキング・カリング用）
    bool getWorldAABB(Vector3& outCenter, Vector3& outExtents) const;

    //! モデル取得
    Model* getModel() const { return m_model.get(); }

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
    };

    //! マテリアル CBV 構造体
    struct MaterialCB
    {
        Vector4 diffuse = {};
        Vector3 pbr = { 0.0f, 0.5f, 1.0f };
        float padding1 = 0.0f;
    };

    //! FBX ファイルを読み込む
    bool loadFbx(const std::string& fbxPath);

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

    std::unique_ptr<ConstantBuffer<ModelCB>> m_modelCB;
    std::unique_ptr<ConstantBuffer<MaterialCB>> m_materialCB;
    size_t m_solidPSOKey = 0;
    size_t m_wireframePSOKey = 0;
    size_t m_gbufferPSOKey = 0;
    DebugMode m_debugMode = DebugMode::None;
    TransformComponent* m_transform = nullptr;
    std::unique_ptr<Model> m_model;
};