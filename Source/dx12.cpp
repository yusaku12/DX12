#include "pch.h"

DX12* DX12::m_instance = nullptr;

DX12::DX12(HWND hwnd)
    :m_hwnd(hwnd)
{
    //! �C���X�^���X�ݒ�
    _ASSERT_EXPR(m_instance == nullptr, "already instantiated");
    m_instance = this;

    // ��ʂ̃T�C�Y���擾����B
    RECT rc;
    GetClientRect(hwnd, &rc);
    UINT screenWidth = rc.right - rc.left;
    UINT screenHeight = rc.bottom - rc.top;

    this->m_width = screenWidth;
    this->m_height = screenHeight;

#ifdef _DEBUG
    //! DX12�Ŏg�p����f�o�b�O�@�\
    enableDebugLayer();
#endif

    //! �t�B�[�`�����x����
    D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    //! DXGI �t�@�N�g���iIDXGIFactory�j ���쐬
    Microsoft::WRL::ComPtr<IDXGIFactory6> m_dxgiFactory;
    HRESULT result = CreateDXGIFactory2(0, IID_PPV_ARGS(m_dxgiFactory.GetAddressOf()));

    //! NVIDIA�̃A�_�v�^��T��
    Microsoft::WRL::ComPtr<IDXGIAdapter>selectedAdapter;
    for (UINT i = 0; ; ++i)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        if (m_dxgiFactory->EnumAdapters(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND)
            break;

        DXGI_ADAPTER_DESC desc = {};
        adapter->GetDesc(&desc);

        std::wstring wdesc = desc.Description;
        if (wdesc.find(L"NVIDIA") != std::wstring::npos)
        {
            selectedAdapter = adapter;
            break;
        }
    }

    //! Direct3D�f�o�C�X�̏�����
    D3D_FEATURE_LEVEL featureLevel;
    for (auto level : levels)
    {
        result = D3D12CreateDevice(selectedAdapter.Get(), level, IID_PPV_ARGS(m_device.GetAddressOf()));
        if (SUCCEEDED(result))
        {
            featureLevel = level;
            break;
        }
    }

    //! �R�}���h���X�g�ƃR�}���h�A���P�[�^�[���쐬
    result = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf()));
    result = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(m_graphicsCommandList.GetAddressOf()));

    //! �R�}���h�L���[�쐬
    D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
    cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;         //!< �^�C���A�E�g�Ȃ�
    cmdQueueDesc.NodeMask = 0;                                  //!< �A�_�v�^�[��1�����g��Ȃ��Ƃ���0�ł���
    cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;//!< �v���C�I���e�B���Ɏw��Ȃ�
    cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;         //!< �����̓R�}���h���X�g�ƍ��킹��
    result = m_device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(m_commandQueue.GetAddressOf()));

    //! �X���b�v�`�F�C���쐬
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = screenWidth;
    swapchainDesc.Height = screenHeight;
    swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  //!< HDR �̏ꍇ�� DXGI_FORMAT_R16G16B16A16_FLOAT
    swapchainDesc.Stereo = false;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
    swapchainDesc.BufferCount = BUFFER_COUNT;
    swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    result = m_dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(),
        m_hwnd,
        &swapchainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(m_dxgiSwapChain4.GetAddressOf()));

    //! RTV�̃f�B�X�N���v�^�q�[�v���쐬
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;  //! �����_�[�^�[�Q�b�g�r���[�Ȃ̂�RTV
    heapDesc.NodeMask = 0;
    heapDesc.NumDescriptors = BUFFER_COUNT;          //! BufferCount�̐��ɍ��킹��
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//! �V�F�[�_�[����f�[�^��ǂݎ��킯�ł͖����̂�NONE
    result = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvHeaps.GetAddressOf()));

    //! SRV�̃f�B�X�N���v�^�q�[�v���쐬
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 64;
    desc.NodeMask = 0;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    result = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_srvHeaps.GetAddressOf()));
    m_exampleDescriptorHeapAllocator.Create(m_device.Get(), m_srvHeaps.Get());  // @todo imgui�p�ꎞ�I�ȃA���P�[�^

    //! �X���b�v�`�F�C���ɕR�Â��� RTV ���쐬
    DXGI_SWAP_CHAIN_DESC swcDesc = {};
    result = m_dxgiSwapChain4->GetDesc(&swcDesc);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = swcDesc.BufferDesc.Format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    for (UINT i = 0; i < swcDesc.BufferCount; ++i)
    {
        result = m_dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(m_backBuffers[i].GetAddressOf()));
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvDesc, handle);
        handle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    //! �t�F���X���쐬(GPU���̏����������������m�邽�߂̎d�g��)
    result = m_device->CreateFence(m_fenceVall, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf()));
}

void DX12::screenClear()
{
    //! ���݂̃o�b�N�o�b�t�@�̃C���f�b�N�X���擾
    UINT bbIdx = m_dxgiSwapChain4->GetCurrentBackBufferIndex();

    //! ���݂�RTV�n���h�����擾
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += bbIdx * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    //! ���\�[�X�o���A: Present �� RenderTarget �ɕύX
    D3D12_RESOURCE_BARRIER barrierDesc = {};
    barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierDesc.Transition.pResource = m_backBuffers[bbIdx].Get();
    barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_graphicsCommandList->ResourceBarrier(1, &barrierDesc);

    //! �����_�[�^�[�Q�b�g��ݒ�
    m_graphicsCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    //! �f�B�X�N���v�^�q�[�v��n��
    ID3D12DescriptorHeap* heaps[] = { m_srvHeaps.Get() };
    m_graphicsCommandList->SetDescriptorHeaps(1, heaps);

    //! ��ʂ��N���A
    FLOAT clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_graphicsCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    //! imgui�̕`�����ݒ�
    IMGUI_CTRL_RENDER_INFO();

    //! ���\�[�X�o���A: RenderTarget �� Present �ɖ߂�
    barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_graphicsCommandList->ResourceBarrier(1, &barrierDesc);

    //! �R�}���h���X�g����Ď��s
    m_graphicsCommandList->Close();
    ID3D12CommandList* lists[] = { m_graphicsCommandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(lists), lists);
}

void DX12::screenClearCleanup()
{
    //! GPU�ҋ@
    safeGPUWait();

    //! �R�}���h���Z�b�g
    commandReset();

    //! �t���b�v����
    m_dxgiSwapChain4->Present(1, 0);
}

void DX12::screenResize(int width, int height)
{
    if (!m_device || !m_dxgiSwapChain4) return;

    //! �R�}���h���X�g�� Close�i�L�^���Ȃ�K������j
    m_graphicsCommandList->Close();

    //! GPU�ҋ@
    safeGPUWait();

    //! �o�b�N�o�b�t�@�����
    for (UINT i = 0; i < BUFFER_COUNT; ++i)
    {
        m_backBuffers[i].Reset();
    }

    //! �o�b�t�@�����T�C�Y
    DXGI_SWAP_CHAIN_DESC oldDesc = {};
    HRESULT hr = m_dxgiSwapChain4->GetDesc(&oldDesc);
    if (FAILED(hr)) Logger::getInstance().logCall(LogLevel::ASSERT, "GetDesc failed before ResizeBuffers");

    hr = m_dxgiSwapChain4->ResizeBuffers(
        oldDesc.BufferCount,
        width,
        height,
        oldDesc.BufferDesc.Format,
        oldDesc.Flags
    );
    if (FAILED(hr)) Logger::getInstance().logCall(LogLevel::ASSERT, "ResizeBuffers failed");

    //! Resize ��ɍĎ擾���čŐV���𔽉f
    DXGI_SWAP_CHAIN_DESC newDesc = {};
    hr = m_dxgiSwapChain4->GetDesc(&newDesc);
    if (FAILED(hr)) Logger::getInstance().logCall(LogLevel::ASSERT, "GetDesc failed after ResizeBuffers");

    //! �����ێ�����T�C�Y���X�V
    m_width = width;
    m_height = height;

    //! RTV�Đ����i�X���b�v�`�F�C���̃t�H�[�}�b�g���g�p�j
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = newDesc.BufferDesc.Format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    for (UINT i = 0; i < newDesc.BufferCount; ++i)
    {
        hr = m_dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(m_backBuffers[i].ReleaseAndGetAddressOf()));
        if (FAILED(hr)) Logger::getInstance().logCall(LogLevel::ASSERT, "GetBuffer failed after ResizeBuffers");

        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvDesc, handle);
        handle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    //! �R�}���h���Z�b�g
    commandReset();
}

void DX12::enableDebugLayer()
{
    Microsoft::WRL::ComPtr<ID3D12Debug> debugLayer;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugLayer.GetAddressOf()))))
    {
        debugLayer->EnableDebugLayer();
    }
}

void DX12::safeGPUWait()
{
    //! Signal �ɂ�� fence �ɒl��ݒ�
    const UINT64 fenceValueToWait = ++m_fenceVall;
    m_commandQueue->Signal(m_fence.Get(), fenceValueToWait);

    //! �����l���҂��l��菬�����ꍇ�̂ݑ҂�
    if (m_fence->GetCompletedValue() < fenceValueToWait)
    {
        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (event == nullptr) Logger::getInstance().logCall(LogLevel::ASSERT, "CreateEvent failed in safeGPUWait");

        m_fence->SetEventOnCompletion(fenceValueToWait, event);
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }
}

void DX12::commandReset()
{
    //! �̈���N���A�A���t���[���p�ɖ��߂�ς߂��Ԃɂ���
    m_commandAllocator->Reset();
    m_graphicsCommandList->Reset(m_commandAllocator.Get(), nullptr);
}