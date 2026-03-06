#pragma once

//=====================================================
// CommandListPool
// - ID3D12GraphicsCommandList と ID3D12CommandAllocator のプール
// - コマンドリストは使用中かどうかを管理して、必要に応じて再利用する
// - コマンドリストは使用後に明示的に release() する必要がある
//=====================================================
class CommandListPool
{
public:

    static CommandListPool& Instance()
    {
        static CommandListPool instance;
        return instance;
    }

    //! コマンドリストとアロケータのセット
    struct CommandListEntry
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
        bool inUse = false;
        bool closed = false;
    };

    //! 初期化
    void initialize(ID3D12Device* device, UINT poolSize = 4);

    //! コマンドリストを取得（使用中でないものがあれば再利用、なければ新規作成）
    ID3D12GraphicsCommandList* acquire();

    //! コマンドリストを使用後に返却（コマンドリストはこの時点ではまだ閉じていないことが多い）
    void release(ID3D12GraphicsCommandList* cmdList);

    //! コマンドリストが閉じられたことを通知（コマンドリストはこの時点で閉じている必要がある）
    std::vector<ID3D12CommandList*> getClosedCommandLists() const;

    //! プール内のすべてのコマンドリストをリセットして再利用可能にする（フレーム開始時などに呼ぶ）
    void resetAll();

    //! 現在プール内で使用中のコマンドリストの数を取得（デバッグ用）
    UINT getActiveCount() const;

private:

    CommandListPool() = default;
    ~CommandListPool() = default;
    CommandListPool(const CommandListPool&) = delete;
    CommandListPool& operator=(const CommandListPool&) = delete;

    ID3D12Device* m_device = nullptr;
    std::vector<CommandListEntry> m_pool;
    mutable std::mutex m_mutex;
};
