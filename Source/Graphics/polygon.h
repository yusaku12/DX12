#pragma once

//=====================================================
// Polygon クラス
//=====================================================
class Polygon
{
public:

    explicit Polygon();
    ~Polygon();

    //! 描画
    void render();

private:

    //! 頂点構造体
    struct Vertex
    {
        Vector3 pos;
        Vector2 uv;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    D3D12_INDEX_BUFFER_VIEW ibView = {};
    LoadTexture* tex = nullptr;
};