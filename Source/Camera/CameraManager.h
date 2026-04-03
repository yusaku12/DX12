#pragma once

#include "Graphics/ConstantBuffer.h"

class CameraComponent;

//=====================================================
// カメラマネージャ(シングルトン)
// - CameraComponent を使ったコンポーネントベースの
//   カメラシステムをサポート
// - CameraComponent が登録されていない場合は
//   内蔵のフォールバックカメラで動作する
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

    //! 初期化
    void initialize();

    //! カメラ更新（GPU 定数バッファ更新を含む）
    void update();

    //! デバック機能
    void debugImgui();

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
        Vector3 cameraPos;      //!< カメラ座標
        float padding;          //!< パディング
    };

    //! 登録済み CameraComponent 一覧
    std::vector<CameraComponent*> m_cameras;

    std::unique_ptr<ConstantBuffer<GPUCameraBuffer>> m_cameraCB;  //!< GPU 定数バッファ
};