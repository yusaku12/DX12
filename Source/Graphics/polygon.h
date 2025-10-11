#pragma once

//=====================================================
// Polygon クラス
//=====================================================
class Polygon
{
public:

    Polygon();
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
    Microsoft::WRL::ComPtr<ID3DBlob> m_rootSigBlob;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    D3D12_INDEX_BUFFER_VIEW ibView = {};
};