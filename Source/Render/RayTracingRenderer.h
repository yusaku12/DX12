#pragma once

struct RenderPassContext;
class FbxRenderComponent;

//=====================================================
//! DXR レンダラー（TLAS/BLAS + RTPSO + Dispatch）
//=====================================================
class RayTracingRenderer
{
public:

    static RayTracingRenderer& Instance()
    {
        static RayTracingRenderer instance;
        return instance;
    }

    void initialize();
    void resize(UINT width, UINT height);

    //! RayTracing パス実行（成功時は finalSrvIndex を更新）
    bool execute(RenderPassContext& context);

    bool isSupported() const { return m_supported; }
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    UINT getOutputSrvIndex() const { return m_outputSrvIndex; }

private:

    RayTracingRenderer() = default;

    struct Vertex
    {
        float x;
        float y;
        float z;
    };

    struct GeometryBuffers
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> vb;
        Microsoft::WRL::ComPtr<ID3D12Resource> ib;
        UINT vertexCount = 0;
        UINT indexCount = 0;
    };

    bool initializeDevice();
    bool createOutputResources(UINT width, UINT height);

    bool buildBottomLevelAS(ID3D12GraphicsCommandList4* cmd4);
    bool buildTopLevelAS(ID3D12GraphicsCommandList4* cmd4);
    bool gatherSceneGeometry(std::vector<GeometryBuffers>& outGeometries) const;

    bool createGlobalRootSignature();
    bool createRtPipelineState();
    bool createShaderTable();
    bool compileDxilLibraryFromHlsl(const std::wstring& hlslPath, std::vector<uint8_t>& outBinary) const;

    Microsoft::WRL::ComPtr<ID3D12Resource> createBuffer(
        UINT64 size,
        D3D12_RESOURCE_FLAGS flags,
        D3D12_RESOURCE_STATES initialState,
        D3D12_HEAP_TYPE heapType) const;

    bool loadDxilLibrary(const std::wstring& path, std::vector<uint8_t>& outBinary) const;

    bool m_initialized = false;
    bool m_supported = false;
    bool m_enabled = true;
    bool m_warnedMissingDxil = false;
    mutable bool m_warnedMissingDxc = false;

    UINT m_width = 0;
    UINT m_height = 0;

    Microsoft::WRL::ComPtr<ID3D12Device5> m_device5;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_globalRootSignature;
    Microsoft::WRL::ComPtr<ID3D12StateObject> m_rtStateObject;
    std::vector<uint8_t> m_rtDxilBinary;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_shaderTable;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_outputTexture;
    D3D12_RESOURCE_STATES m_outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    UINT m_outputSrvIndex = UINT_MAX;
    UINT m_outputUavIndex = UINT_MAX;

    std::vector<GeometryBuffers> m_geometryBuffers;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_blasScratch;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_blas;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_instanceDescBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlasScratch;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlas;

    UINT m_lastGeometryHash = 0;
};
