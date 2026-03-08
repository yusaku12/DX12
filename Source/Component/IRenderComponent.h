#pragma once

#include <d3d12.h>

class TransformComponent;

//=====================================================
// 描画可能コンポーネントのインターフェース
// RenderManager に登録される単位
//=====================================================
class IRenderComponent
{
public:

    virtual ~IRenderComponent() = default;

    //! 描画（メインコマンドリスト）
    virtual void render() = 0;

    //! 描画（指定コマンドリスト — マルチスレッド用）
    virtual void render(ID3D12GraphicsCommandList* cmd) = 0;

    //! デバッグ用の名前
    virtual const char* getRenderName() const = 0;
};
