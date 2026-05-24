#pragma once

#include "Graphics/ConstantBuffer.h"

class CameraComponent;
enum class RenderPath : int;
enum class RenderPassFlags : unsigned int;

//=====================================================
// カメラマネージャ（シングルトン）
//=====================================================
class CameraManager
{
public:

    //! シングルトンインスタンス取得
    static CameraManager& Instance()
    {
        static CameraManager instance;
        return instance;
    }

    //! CameraComponent を登録（CameraComponent::start / onEnable から呼ばれる）
    void registerCamera(CameraComponent* cam);

    //! CameraComponent を解除（CameraComponent::onDisable / onDestroy から呼ばれる）
    void unregisterCamera(CameraComponent* cam);

    //! アクティブなメインカメラを返す（depth 最大、なければ nullptr）
    CameraComponent* getMainCamera() const;

    //! メインカメラの描画パス取得
    RenderPath getMainRenderPath() const;

    //! メインカメラの描画パスマスク取得
    RenderPassFlags getMainRenderPassMask() const;

    //! 初期化
    void initialize();

    //! 終了処理
    void shutdown();

    //! カメラ更新（GPU 定数バッファ更新を含む）
    void update();

    //! カメラ定数バッファの GPU アドレス取得
    D3D12_GPU_VIRTUAL_ADDRESS getGPUAddress() const { return m_cameraCB->getGPUAddress(); }

private:

    //! GPU 定数バッファを現在のメインカメラのデータで更新
    void uploadCameraBufferToGPU();

    //! 定数バッファ構造体
    struct GPUCameraBuffer
    {
        Matrix view;            //!< ビュー行列
        Matrix projection;      //!< プロジェクション行列
        Matrix viewProjection;  //!< ビュー×プロジェクション行列
        Matrix viewInverse;     //!< ビュー行列の逆行列
        Vector3 cameraPos;      //!< カメラ座標
        float padding;          //!< パディング
    };

    //! 登録済み CameraComponent 一覧
    std::vector<CameraComponent*> m_cameras;
    std::unique_ptr<ConstantBuffer<GPUCameraBuffer>> m_cameraCB;  //!< GPU 定数バッファ
};