#pragma once

#include "Camera.h"
#include "CameraBehaviour.h"
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
        static CameraManager inst;
        return inst;
    }

    // ─── CameraComponent 登録 API ───────────────────────

    //! CameraComponent を登録（CameraComponent::start / onEnable から呼ばれる）
    void registerCamera(CameraComponent* cam);

    //! CameraComponent を解除（CameraComponent::onDisable / onDestroy から呼ばれる）
    void unregisterCamera(CameraComponent* cam);

    //! アクティブなメインカメラを返す（depth 最大、なければ nullptr）
    CameraComponent* getMainCamera() const;

    // ─── フォールバック用カメラ挙動 ─────────────────────

    //! フォールバックカメラの挙動を差し替える（CameraComponent 未使用時のみ有効）
    void setBehaviour(std::unique_ptr<CameraBehaviour> behaviour);

    // ─── ライフサイクル ───────────────────────────────────

    //! 初期化
    void initialize();

    //! カメラ更新（GPU 定数バッファ更新を含む）
    void update();

    //! デバック機能
    void debugImgui();

    // ─── 行列・GPU アドレス取得（既存コードとの互換） ──

    //! ビュー行列取得
    DirectX::SimpleMath::Matrix getView() const;

    //! プロジェクション行列取得
    DirectX::SimpleMath::Matrix getProjection() const;

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

    //! フォールバックカメラ（CameraComponent が一つも登録されていない場合に使う）
    Camera m_fallbackCamera;

    std::unique_ptr<ConstantBuffer<GPUCameraBuffer>> m_cameraCB;  //!< GPU 定数バッファ
    std::unique_ptr<CameraBehaviour>                 m_behaviour;  //!< フォールバック挙動
};