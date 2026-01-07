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

    //! カメラ取得
    Camera m_camera;

    //! カメラ挙動設定
    void setBehaviour(std::unique_ptr<CameraBehaviour> behaviour);

    //! 初期化
    void initialize();

    //! カメラ更新
    void update();

private:

    //! GPUに送るカメラ定数バッファ
    void uploadCameraBufferToGPU();

    //! ルートシグネチャ作成
    void createRootSignature();

    //! 定数バッファ構造体
    struct GPUCameraBuffer
    {
        Matrix view;        //!< ビュー行列
        Matrix projection;  //!< プロジェクション行列
        Vector3 cameraPos;  //!< カメラ座標
    };

    ConstantBuffer<GPUCameraBuffer> m_cameraCB; //!< カメラ定数バッファ
    std::unique_ptr<CameraBehaviour> m_behaviour;
};