#pragma once

#include "Camera.h"
#include "CameraBehaviour.h"
#include "Graphics/ConstantBuffer.h"

//=====================================================
// カメラマネージャ(シングルトン)
//=====================================================
class CameraManager
{
public:

    //! シングルトンインスタンス取得
    static CameraManager& Instance()
    {
        static CameraManager inst;
        return inst;
    }

    //! カメラ挙動設定
    void setBehaviour(std::unique_ptr<CameraBehaviour> behaviour);

    //! 初期化
    void initialize();

    //! カメラ更新
    void update();

    //! デバック機能
    void debugImgui();

    //! ビュー行列取得
    DirectX::SimpleMath::Matrix getView() const { return m_camera.getView(); }

    //! プロジェクション行列取得
    DirectX::SimpleMath::Matrix getProjection() const { return m_camera.getProjection(); }

    //! カメラ定数バッファのGPUアドレス取得
    D3D12_GPU_VIRTUAL_ADDRESS getGPUAddress() const { return m_cameraCB->getGPUAddress(); }

private:

    //! GPUに送るカメラ定数バッファ
    void uploadCameraBufferToGPU();

    //! 定数バッファ構造体
    struct GPUCameraBuffer
    {
        Matrix view;           //!< ビュー行列
        Matrix projection;     //!< プロジェクション行列
        Matrix viewProjection; //!< ビュー×プロジェクション行列
        Vector3 cameraPos;     //!< カメラ座標
        float padding;         //!< パディング
    };

    //! カメラ取得
    Camera m_camera;

    std::unique_ptr<ConstantBuffer<GPUCameraBuffer>> m_cameraCB; //!< カメラ定数バッファ
    std::unique_ptr<CameraBehaviour> m_behaviour;
};