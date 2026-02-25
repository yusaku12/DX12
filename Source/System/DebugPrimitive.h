#pragma once

#include "Graphics\PSOCreator.h"
#include "Graphics\UploadBuffer.h"

//=====================================================
// デバッグプリミティブ（ライン中心）
// - 大量生成を想定した動的バッファでバッチ描画
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

    //!< 最大ライン数（デフォルト 65536）
    void initialize(UINT maxLines = 65536);

    //!< 終了処理
    void shutdown();

    //!< フレーム開始前に呼び出すこと（内部ステートクリアも行う）
    void beginFrame();

    //! ラインの寿命更新（duration > 0 用））
    void update(float deltaTime);

    //! 描画コマンド発行（呼び出し側で DescriptorHeap/CBV は不要）
    void render();

    //! ライン追加（duration > 0 でフレーム持続、0でフレームのみ）
    void addLine(const Vector3& a, const Vector3& b, const Vector4& color = Vector4(1, 1, 1, 1), float duration = 0.0f);

    //! 箱追加（duration > 0 でフレーム持続、0でフレームのみ）
    void addBox(const Vector3& center, const Vector3& extents, const Vector4& color = Vector4(1, 1, 1, 1), float duration = 0.0f);

    //! 座標フレーム追加（duration > 0 でフレーム持続、0でフレームのみ）
    void addTransform(const Matrix& transform, float size = 1.0f);

    //! 球体（wireframe）追加
    //! segments: 経度分割、rings: 緯度分割（両方とも最低値 3）
    void addSphere(const Vector3& center, float radius, int segments = 24, int rings = 12, const Vector4& color = Vector4(1, 1, 1, 1), float duration = 0.0f);

    //! モデルエディタ用グリッド追加（XZ平面）
    //! step: 格子幅（>0）、width/depth: 全体サイズ（正の値）
    void addGrid(const Vector3& center, float width, float depth, float step = 1.0f, const Vector4& color = Vector4(0.5f, 0.5f, 0.5f, 1.0f), float duration = 0.0f);

    //! 内部ステートクリア（フレーム開始前に呼び出すこと）
    void clear();

private:

    DebugPrimitive() = default;
    ~DebugPrimitive() = default;
    DebugPrimitive(const DebugPrimitive&) = delete;
    DebugPrimitive& operator=(const DebugPrimitive&) = delete;

    //! ラインデータ
    struct Line
    {
        Vector3 a;
        Vector3 b;
        Vector4 color;
        float remaining = 0.0f; //!< 0 => frame-only (描画後即消える)
    };

    //! 頂点データ
    struct Vertex
    {
        Vector3 pos;
        Vector4 color;
    };

    //! バッファ確保
    void ensureBuffer();

    //! 頂点バッファ構築
    void buildVertexBuffer();

    //! 内部実装
    UINT m_maxLines = 0;
    std::vector<Line> m_lines;

    //! 動的頂点バッファ（UploadHeap）
    std::unique_ptr<UploadBuffer> m_uploadBuffer;
    Vertex* m_mappedVertices = nullptr;
    UINT m_vertexBufferSize = 0;
    D3D12_VERTEX_BUFFER_VIEW m_vbv = {};

    //! PSO/リソース
    std::unique_ptr<PSOCreator> m_psoCreator;
};