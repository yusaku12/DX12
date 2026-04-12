#pragma once

//=====================================================
//! スクリーンキャプチャユーティリティ
//! シーンレンダーターゲットまたはバックバッファを
//! PNG ファイルとして保存する
//=====================================================
class ScreenCapture
{
public:

    //! シングルトンインスタンス取得
    static ScreenCapture& Instance()
    {
        static ScreenCapture instance;
        return instance;
    }

    //! スクリーンショットリクエスト（次フレームで実行）
    void requestCapture() { m_captureRequested = true; }

    //! キャプチャが要求されているか
    bool isCaptureRequested() const { return m_captureRequested; }

    //! GPU リソースからピクセルデータを読み取り PNG 保存
    //! @param resource   キャプチャ対象のリソース（Scene RT など）
    //! @param state      リソースの現在の状態
    //! @param cmdList    コマンドリスト
    //! @param cmdQueue   コマンドキュー
    //! @param device     デバイス
    void capture(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES currentState,
        ID3D12GraphicsCommandList* cmdList,
        ID3D12CommandQueue* cmdQueue,
        ID3D12Device* device);

private:

    ScreenCapture() = default;
    ~ScreenCapture() = default;
    ScreenCapture(const ScreenCapture&) = delete;
    ScreenCapture& operator=(const ScreenCapture&) = delete;

    //! タイムスタンプ付きファイルパスを生成
    std::wstring generateFilePath() const;

    bool m_captureRequested = false;
};