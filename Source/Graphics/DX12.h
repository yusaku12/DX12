#pragma once

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

enum class RenderPath : int;

//=====================================================
//! ImGui 用アロケータ
//=====================================================
struct ExampleDescriptorHeapAllocator
{
    struct DescriptorAllocation
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
        UINT index = DescriptorIndexInvalid;

        bool IsValid() const { return index != DescriptorIndexInvalid; }
        static constexpr UINT DescriptorIndexInvalid = UINT_MAX;
    };

    ID3D12DescriptorHeap* Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu{};
    UINT HeapHandleIncrement = 0;
    std::vector<UINT> FreeIndices;

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
    {
        assert(Heap == nullptr && FreeIndices.empty());
        Heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        HeapType = desc.Type;
        HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
        FreeIndices.reserve(desc.NumDescriptors);
        for (int n = (int)desc.NumDescriptors - 1; n >= 0; --n)
            FreeIndices.push_back(static_cast<UINT>(n));
    }

    void Destroy()
    {
        Heap = nullptr;
        FreeIndices.clear();
    }

    DescriptorAllocation Allocate()
    {
        assert(!FreeIndices.empty());
        DescriptorAllocation out;
        out.index = FreeIndices.back();
        FreeIndices.pop_back();
        out.cpuHandle.ptr = HeapStartCpu.ptr + (static_cast<SIZE_T>(out.index) * HeapHandleIncrement);
        out.gpuHandle.ptr = HeapStartGpu.ptr + (static_cast<SIZE_T>(out.index) * HeapHandleIncrement);
        return out;
    }

    void FreeByIndex(UINT index)
    {
        // 簡易検査（ここは追加の検査を入れると堅牢）
        FreeIndices.push_back(index);
    }
};

//=====================================================
// DX12 管理クラス（アプリ内シングルトンとして使用）
//=====================================================
class DX12
{
public:

    DX12(HWND hwnd);
    ~DX12() = default;

    //! シングルトン取得
    static DX12& Instance() { return *m_instance; };

    //! 初期化
    void initialize();

    //! 画面をクリア（Scene 用レンダーターゲットへ）
    void screenClear(RenderPath renderPath);

    //! シーンのimgui描画
    void sceneImguiRender();

    //! 画面クリアの後処理（SRVに戻してPresentなど）
    void screenClearCleanup();

    //! 画面をリサイズ
    void screenResize(int width, int height);

    //! バックバッファをimgui用に準備
    void prepareBackBufferForImGui();

    //! 現在のビューポートとシザー矩形をコマンドリストに設定
    void applyViewportAndScissor(ID3D12GraphicsCommandList* cmd) const;

    //! 現在の RenderTarget（Scene RT + DSV）をコマンドリストに設定
    void applySceneRenderTargets(ID3D12GraphicsCommandList* cmd) const;

    //! 深度バッファを SRV に遷移
    void transitionDepthToSRV();

    //! 深度バッファを DEPTH_WRITE に遷移
    void transitionDepthToWrite();

    //! フェンスを待つ
    void safeGPUWait();

    //! スクリーンショットを撮影（Scene RT を PNG として保存）
    void captureScreenshot();

    //! Scene RT を RENDER_TARGET に遷移
    void transitionSceneToRenderTarget();

    //! デバイス取得
    ID3D12Device* getDevice() const { return m_device.Get(); }

    //! コマンドキュー取得
    ID3D12CommandQueue* getCommandQueue() const { return m_commandQueue.Get(); }

    //! レンダーターゲットのディスクリプタヒープ
    ID3D12DescriptorHeap* getRTVDescriptorHeap() const { return m_rtvHeaps.Get(); }

    //! コマンドリスト取得
    ID3D12GraphicsCommandList* getGraphicsCommandList() const { return m_graphicsCommandList.Get(); }

    //! バックバッファ取得
    DXGI_FORMAT getBackBufferFormat() const { return m_backBufferFormat; }

    //! DSV ハンドル取得
    D3D12_CPU_DESCRIPTOR_HANDLE getDSVHandle() const { return m_dsvHandle; }

    //! Scene RTV ハンドル取得
    D3D12_CPU_DESCRIPTOR_HANDLE getSceneRTVHandle() const { return m_sceneRTVHandle; }

    //! 深度 SRV インデックス取得
    UINT getDepthSrvIndex() const { return m_depthSrvIndex; }

    //! フェンス取得
    ID3D12Fence* getFence() const { return m_fence.Get(); }

    //! imgui一時的なアロケータ
    ExampleDescriptorHeapAllocator& getExampleDescriptorHeapAllocator() { return m_exampleDescriptorHeapAllocator; }

    //! ハンドル取得
    HWND getHwnd() const { return m_hwnd; }

    //! シーンがアクティブか
    bool isSceneActive() const { return m_isSceneActive; }

    //! スクリーンサイズ取得
    const int& getScreenWidth() const { return m_width; }
    const int& getScreenHeight() const { return m_height; }

    //! Scene ウィンドウ内の描画矩形（スクリーン座標）
    ImVec2 getSceneWindowPos() const { return m_sceneWindowPos; }
    ImVec2 getSceneWindowSize() const { return m_sceneWindowSize; }

    //! Scene ウィンドウの ImDrawList（ImGuizmo が入力判定で参照するため）
    ImDrawList* getSceneDrawList() const { return m_sceneDrawList; }

    //! シーンレンダーターゲット取得
    ID3D12Resource* getSceneRenderTarget() const { return m_sceneRenderTarget.Get(); }

private:

    //! DX12で使用するデバッグ機能
    void enableDebugLayer();

    //! Info Queue 設定（警告フィルタリング）
    void setupInfoQueue();

    //! コマンドリセット
    void commandReset();

    //! コマンドリスト実行
    void executeCommandList();

    //! Scene RT を SRV に遷移（ImGui 描画前に呼ぶ）
    void transitionSceneToSRV();

    //! Debug Layer の警告を出力ウィンドウにフラッシュ
    void flushDebugMessages();

    static DX12* m_instance;
    const HWND m_hwnd;
    int m_width = 1280, m_height = 720;     //!< 画面の縦幅、横幅
    static constexpr int BUFFER_COUNT = 3;  //!< バックバッファの数
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_postCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_postCommandAllocator2;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_graphicsCommandList;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_dxgiSwapChain4;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeaps;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[BUFFER_COUNT];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencil;
    ExampleDescriptorHeapAllocator m_exampleDescriptorHeapAllocator;
    D3D12_RESOURCE_BARRIER m_barrierDesc = {};
    UINT64 m_fenceValue = 0;
    DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_sceneRenderTarget;
    D3D12_CPU_DESCRIPTOR_HANDLE m_sceneRTVHandle{};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle = {};
    UINT m_sceneSrvIndex = 0;
    UINT m_depthSrvIndex = UINT_MAX;
    D3D12_RESOURCE_STATES m_depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    UINT m_finalPostEffectSrv = 0;
    bool m_isSceneActive = false;
    ImVec2 m_sceneWindowPos = ImVec2(0, 0);
    ImVec2 m_sceneWindowSize = ImVec2(0, 0);
    ImDrawList* m_sceneDrawList = nullptr;

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> m_infoQueue;
#endif
};