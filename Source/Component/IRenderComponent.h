#pragma once

#include <d3d12.h>
#include "Component.h"
#include "Model\Model.h"

//=====================================================
// 描画可能コンポーネントのインターフェース
// RenderManager に登録される単位
//=====================================================
class IRenderComponent : public Component
{
public:

    virtual ~IRenderComponent() = default;

    //! ゲーム開始時に一度だけ呼ばれる
    virtual void start() override;

    //! 破棄される直前に呼ばれる
    virtual void onDestroy() override;

    //! 有効化
    virtual void onEnable() override;

    //! 無効化
    virtual void onDisable() override;

    //! 描画(シングルスレッド)
    virtual void render() = 0;

    //! 描画(マルチスレッド)
    virtual void render(ID3D12GraphicsCommandList* cmd) = 0;

    //! GBuffer 描画
    virtual void renderGBuffer(ID3D12GraphicsCommandList* cmd);

    //! Forward 描画
    virtual void renderForward(ID3D12GraphicsCommandList* cmd);

    //! シャドウ深度描画
    virtual void renderShadowDepth(ID3D12GraphicsCommandList* cmd) { (void)cmd; }

    //! 境界ボックス（AABB）の取得（カリングに利用）。有効な境界が存在する場合は true を返す。
    virtual bool getWorldAABB(Vector3& outCenter, Vector3& outExtents) const { (void)outCenter; (void)outExtents; return false; }

    //! ランタイム自動LOD用: カメラ位置から推奨LODレベルを返す（0=最高品質）
    virtual int evaluateAutoLodLevel(const Vector3& cameraPosition) const { (void)cameraPosition; return 0; }

    //! ランタイム描画で使用するLODレベルを設定
    virtual void setRuntimeLodLevel(int lodLevel) { (void)lodLevel; }

    //! このコンポーネントが持つLOD段数（最低1）
    virtual int getLodLevelCount() const { return 1; }
};