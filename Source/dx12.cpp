#include "dx12.h"
#include <stdexcept>
#include <string>
#include "System\logger.h"

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
    HRESULT result = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_dxgiFactory));

    //! NVIDIA�̃A�_�v�^��T��
    Microsoft::WRL::ComPtr<IDXGIAdapter>selectedAdapter;
    for (UINT i = 0; ; ++i)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        if (m_dxgiFactory->EnumAdapters(i, &adapter) == DXGI_ERROR_NOT_FOUND)
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
        result = D3D12CreateDevice(selectedAdapter.Get(), level, IID_PPV_ARGS(&m_device));
        if (SUCCEEDED(result))
        {
            featureLevel = level;
            break;
        }
    }

    //! �R�}���h���X�g�ƃR�}���h�A���P�[�^�[���쐬
    result = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
    result = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_graphicsCommandList));

    //! �R�}���h�L���[�쐬
    D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
    cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;         //!< �^�C���A�E�g�Ȃ�
    cmdQueueDesc.NodeMask = 0;                                  //!< �A�_�v�^�[��1�����g��Ȃ��Ƃ���0�ł���
    cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;//!< �v���C�I���e�B���Ɏw��Ȃ�
    cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;         //!< �����̓R�}���h���X�g�ƍ��킹��
    result = m_device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_commandQueue));

    //! �X���b�v�`�F�C���쐬
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = screenWidth;
    swapchainDesc.Height = screenHeight;
    swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  //!< HDR �̏ꍇ�� DXGI_FORMAT_R16G16B16A16_FLOAT
    swapchainDesc.Stereo = false;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    swapchainDesc.BufferCount = BUFFER_COUNT;
    swapchainDesc.Scaling = DXGI_SCALING_NONE;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
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
    result = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeaps));

    //! SRV�̃f�B�X�N���v�^�q�[�v���쐬
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 64;
    desc.NodeMask = 0;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    result = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeaps));
    m_exampleDescriptorHeapAllocator.Create(m_device.Get(), m_srvHeaps.Get());  // @todo imgui�p�ꎞ�I�ȃA���P�[�^

    //! �X���b�v�`�F�[���ƕR�Â����s��
    DXGI_SWAP_CHAIN_DESC swcDesc = {};
    result = m_dxgiSwapChain4->GetDesc(&swcDesc);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < swcDesc.BufferCount; ++i)
    {
        result = m_dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, handle);
        handle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    //! �t�F���X���쐬(GPU���̏����������������m�邽�߂̎d�g��)
    result = m_device->CreateFence(m_fenceVall, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf()));
}

void DX12::screenClear()
{
    //! �o�b�N�o�b�t�@�̃C���f�b�N�X���擾
    //! ���\�[�X�o���A���쐬�[���o�b�N�o�b�t�@�̏�Ԃ�J��
    auto bbIdx = m_dxgiSwapChain4->GetCurrentBackBufferIndex();
    D3D12_RESOURCE_BARRIER BarrierDesc = {};
    BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    BarrierDesc.Transition.pResource = m_backBuffers[bbIdx].Get();
    BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_graphicsCommandList->ResourceBarrier(1, &BarrierDesc);

    auto rtvH = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    rtvH.ptr += static_cast<ULONG_PTR>(bbIdx * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

    //��ʃN���A
    float clearColor[] = { 0.4f,0.4f,0.7f,1.0f };
    m_graphicsCommandList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

    //! �����_�[�^�[�Q�b�g���w��
    m_graphicsCommandList->OMSetRenderTargets(1, &rtvH, false, nullptr);
    m_graphicsCommandList->SetDescriptorHeaps(1, m_srvHeaps.GetAddressOf());

    //! imgui�̕`�����ݒ�
    IMGUI_CTRL_RENDER_INFO();

    //! ��ʂ�J�ڂ�����ɐ؂�ւ��錳�ɖ߂���������(�߂�ǂ�����)
    BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_graphicsCommandList->ResourceBarrier(1, &BarrierDesc);

    //! ���߂̋L�^�͏I���
    m_graphicsCommandList->Close();

    //! �R�}���h���X�g�̎��s
    m_commandList = m_graphicsCommandList;
    m_commandQueue->ExecuteCommandLists(1, m_commandList.GetAddressOf());
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
    DXGI_SWAP_CHAIN_DESC desc = {};
    m_dxgiSwapChain4->GetDesc(&desc);
    HRESULT hr = m_dxgiSwapChain4->ResizeBuffers(
        desc.BufferCount,
        width,
        height,
        desc.BufferDesc.Format,
        desc.Flags
    );
    if (FAILED(hr)) throw std::runtime_error("ResizeBuffers failed");

    //! RTV�Đ���
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < desc.BufferCount; ++i)
    {
        hr = m_dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        if (FAILED(hr)) throw std::runtime_error("GetBuffer failed");

        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, handle);
        handle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    //! �R�}���h���Z�b�g
    commandReset();
}

void DX12::enableDebugLayer()
{
    ID3D12Debug* debugLayer = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer))))
    {
        debugLayer->EnableDebugLayer();
        debugLayer->Release();
    }
}

void DX12::safeGPUWait()
{
    UINT64 fenceValueToWait = ++m_fenceVall;

    m_commandQueue->Signal(m_fence.Get(), fenceValueToWait);

    UINT64 completed = m_fence->GetCompletedValue();
    if (completed >= fenceValueToWait) return; // GPU �����ς݂Ȃ�҂��Ȃ�

    HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle) throw std::runtime_error("Failed to create event");

    HRESULT hr = m_fence->SetEventOnCompletion(fenceValueToWait, eventHandle);
    if (FAILED(hr))
    {
        CloseHandle(eventHandle);
        throw std::runtime_error("Failed to set fence event");
    }

    WaitForSingleObject(eventHandle, INFINITE);
    CloseHandle(eventHandle);
}

void DX12::commandReset()
{
    //! �̈���N���A�A���t���[���p�ɖ��߂�ς߂��Ԃɂ���
    m_commandAllocator->Reset();
    m_graphicsCommandList->Reset(m_commandAllocator.Get(), nullptr);
}