#include "pch.h"
#include "ScreenCapture.h"
#include <wincodec.h>

void ScreenCapture::capture(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES currentState,
    ID3D12GraphicsCommandList* cmdList,
    ID3D12CommandQueue* cmdQueue,
    ID3D12Device* device)
{
    m_captureRequested = false;

    if (!resource || !cmdList || !cmdQueue || !device)
    {
        LOG_ERROR("ScreenCapture: 無効な引数");
        return;
    }

    //! リソースの情報を取得
    D3D12_RESOURCE_DESC desc = resource->GetDesc();
    UINT64 width = desc.Width;
    UINT height = desc.Height;
    DXGI_FORMAT format = desc.Format;

    //! フットプリント情報取得（行ピッチ含む）
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 totalBytes = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

    //! Readback バッファ作成
    Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_READBACK);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBytes);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(readbackBuffer.GetAddressOf()));
        LOG_HR(hr, "ScreenCapture: Readback バッファ作成失敗");
        if (FAILED(hr)) return;
    }

    //! リソースを COPY_SOURCE に遷移
    if (currentState != D3D12_RESOURCE_STATE_COPY_SOURCE)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource,
            currentState,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        cmdList->ResourceBarrier(1, &barrier);
    }

    //! コピーコマンド発行
    {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = readbackBuffer.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = footprint;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = resource;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;

        cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    //! 元の状態に戻す
    if (currentState != D3D12_RESOURCE_STATE_COPY_SOURCE)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            currentState);
        cmdList->ResourceBarrier(1, &barrier);
    }

    //! コマンドリスト実行 & GPU 待機
    cmdList->Close();
    ID3D12CommandList* lists[] = { cmdList };
    cmdQueue->ExecuteCommandLists(1, lists);

    //! フェンスで GPU 完了を待つ
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));
    LOG_HR(hr, "ScreenCapture: フェンス作成失敗");
    if (FAILED(hr)) return;

    HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!event)
    {
        LOG_ERROR("ScreenCapture: イベント作成失敗");
        return;
    }

    cmdQueue->Signal(fence.Get(), 1);
    fence->SetEventOnCompletion(1, event);
    WaitForSingleObject(event, INFINITE);
    CloseHandle(event);

    //! Readback バッファからピクセルデータを読み取り
    void* mappedData = nullptr;
    hr = readbackBuffer->Map(0, nullptr, &mappedData);
    LOG_HR(hr, "ScreenCapture: Map 失敗");
    if (FAILED(hr)) return;

    //! DirectXTex の Image を構築して保存
    DirectX::Image image = {};
    image.width = static_cast<size_t>(width);
    image.height = static_cast<size_t>(height);
    image.format = format;
    image.rowPitch = static_cast<size_t>(footprint.Footprint.RowPitch);
    image.slicePitch = image.rowPitch * height;
    image.pixels = static_cast<uint8_t*>(mappedData);

    //! 保存先ディレクトリの作成
    std::filesystem::create_directories("Screenshots");

    //! PNG として保存
    std::wstring filePath = generateFilePath();
    hr = DirectX::SaveToWICFile(
        image,
        DirectX::WIC_FLAGS_NONE,
        GUID_ContainerFormatPng,
        filePath.c_str());

    readbackBuffer->Unmap(0, nullptr);

    if (SUCCEEDED(hr))
    {
        //! ワイド文字をマルチバイトに変換してログ出力
        char narrowPath[512] = {};
        WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, narrowPath, sizeof(narrowPath), nullptr, nullptr);
        LOG_INFO("スクリーンショット保存: %s", narrowPath);
    }
    else
    {
        LOG_ERROR("スクリーンショット保存失敗 (HRESULT: 0x%08X)", hr);
    }
}

std::wstring ScreenCapture::generateFilePath() const
{
    //! 現在日時でファイル名を生成
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm localTime = {};
    localtime_s(&localTime, &time);

    std::wostringstream oss;
    oss << L"Screenshots/Screenshot_"
        << std::put_time(&localTime, L"%Y%m%d_%H%M%S")
        << L"_" << std::setfill(L'0') << std::setw(3) << ms.count()
        << L".png";

    return oss.str();
}