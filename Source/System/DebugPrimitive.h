#pragma once

#include "Graphics\PSOCreator.h"
#include "Graphics\UploadBuffer.h"
#include "Graphics\ConstantBuffer.h"

//=====================================================
// デバッグプリミティブ
// - 初期化時に単位メッシュ（球・半球・円柱・箱）を頂点バッファとして作成
// - 描画時はワールド行列 + スケールで GPU 側で変換（CPU 展開なし）
//=====================================================
class DebugPrimitive
{
public:

    //! シングルトンインスタンス取得
    static DebugPrimitive& Instance()
    {
        static DebugPrimitive instance;
        return instance;
    }

    //! 頂点構造体（位置 + カラー）
    struct Vertex
    {
        Vector3 position;
        Vector4 color;
    };

    //! 初期化（メッシュ作成・PSO登録）
    void initialize();

    //! 終了処理
    void shutdown();

    //! フレーム開始前に呼び出すこと（描画リクエストをクリア）
    void beginFrame();

    //! 描画コマンド発行
    void render();

    //! 球描画
    void drawSphere(const Matrix& world, float radius, const Vector4& color = { 0, 1, 0, 1 });

    //! 箱描画
    void drawBox(const Matrix& world, const Vector3& extents, const Vector4& color = { 0, 1, 0, 1 });

    //! カプセル描画（Y軸方向）
    void drawCapsule(const Matrix& world, float radius, float halfHeight, const Vector4& color = { 0, 1, 0, 1 });

    //! 円柱描画
    void drawCylinder(const Matrix& world, float radius, float height, const Vector4& color = { 0, 1, 0, 1 });

    //! グリッド描画（XZ平面）
    void drawGrid(const Vector3& center, float width, float depth, float step = 1.0f, const Vector4& color = { 0.5f, 0.5f, 0.5f, 1.0f });

private:

    DebugPrimitive() = default;
    ~DebugPrimitive() = default;
    DebugPrimitive(const DebugPrimitive&) = delete;
    DebugPrimitive& operator=(const DebugPrimitive&) = delete;

    //! GPU に送るメッシュ定数バッファ
    struct CbMesh
    {
        Matrix world;
        Vector4 color;
    };

    //! 描画リクエスト（形状ごとにバッチ）
    struct SphereRequest { Matrix world; float radius; Vector4 color; };
    struct BoxRequest { Matrix world; Vector3 extents; Vector4 color; };
    struct CylinderRequest { Matrix world; float radius; float height; Vector4 color; };

    //! グリッド用のラインリクエスト
    struct LineRequest { Vector3 a; Vector3 b; Vector4 color; };

    //! メッシュ作成
    void createSphereMesh(int subdivisions = 16);
    void createHalfSphereMesh(int subdivisions = 16);
    void createCylinderMesh(int subdivisions = 16);
    void createBoxMesh();
    void createLineMesh();

    //! 頂点バッファ作成ヘルパー
    void createVertexBuffer(const Vertex* vertices, UINT count,
        Microsoft::WRL::ComPtr<ID3D12Resource>& outBuffer,
        D3D12_VERTEX_BUFFER_VIEW& outVBV);

    //! 形状ごとの描画
    void renderSpheres();
    void renderBoxes();
    void renderCylinders();
    void renderCapsules();
    void renderGrid();

    //! 定数バッファ更新＆描画
    void drawMesh(const D3D12_VERTEX_BUFFER_VIEW& vbv, UINT vertexCount, const CbMesh& cb);

    //! 描画リクエスト
    std::vector<SphereRequest>   m_sphereRequests;
    std::vector<SphereRequest>   m_halfSphereRequests;  //!< カプセル用
    std::vector<BoxRequest>      m_boxRequests;
    std::vector<CylinderRequest> m_cylinderRequests;
    std::vector<LineRequest>     m_lineRequests;         //!< グリッド用

    //! 頂点バッファ & VBV
    Microsoft::WRL::ComPtr<ID3D12Resource> m_sphereVB;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_halfSphereVB;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cylinderVB;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_boxVB;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_lineVB; //!< 2頂点のラインメッシュ

    D3D12_VERTEX_BUFFER_VIEW m_sphereVBV = {};
    D3D12_VERTEX_BUFFER_VIEW m_halfSphereVBV = {};
    D3D12_VERTEX_BUFFER_VIEW m_cylinderVBV = {};
    D3D12_VERTEX_BUFFER_VIEW m_boxVBV = {};
    D3D12_VERTEX_BUFFER_VIEW m_lineVBV = {};

    UINT m_sphereVertexCount = 0;
    UINT m_halfSphereVertexCount = 0;
    UINT m_cylinderVertexCount = 0;
    UINT m_boxVertexCount = 0;

    //! メッシュ定数バッファ
    static constexpr UINT MAX_DRAW_CALLS = 512;
    std::unique_ptr<ConstantBuffer<CbMesh>> m_meshCB;
    UINT m_drawIndex = 0;

    //! グリッド用動的頂点バッファ
    std::unique_ptr<UploadBuffer> m_gridUploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_gridVBV = {};
    Vertex* m_gridMapped = nullptr;

    //! PSOキー
    size_t m_meshPsoKey = 0;
};