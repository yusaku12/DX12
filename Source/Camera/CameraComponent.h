#pragma once

#include "Component\Component.h"
#include "Graphics\ConstantBuffer.h"

class TransformComponent;

//=====================================================
// カメラコンポーネント
// UnityEngine.Camera 相当
// - GameObject に付与して使用する
// - TransformComponent が同じ GameObject に存在する場合は
//   その位置・回転を使ってビュー行列を計算する
// - CameraManager に自動登録/解除される
//=====================================================
class CameraComponent : public Component
{
public:

    CameraComponent() = default;
    ~CameraComponent() override = default;

    //! 初期化（awake では他コンポーネントへの参照は取得しない）
    void awake() override;

    //! ゲーム開始時（TransformComponent キャッシュ & CameraManager 登録）
    void start() override;

    //! 有効化されたとき
    void onEnable() override;

    //! 無効化されたとき
    void onDisable() override;

    //! 破棄される直前
    void onDestroy() override;

    //! インスペクタ表示
    void inspectGUI() override;

    // ─── カメラプロパティ ───────────────────────────────

    //! 視野角（ラジアン）の取得
    float getFov() const { return m_fov; }

    //! 視野角（ラジアン）の設定
    void setFov(float fov) { m_fov = fov; }

    //! ニアクリップ距離の取得
    float getNear() const { return m_nearZ; }

    //! ニアクリップ距離の設定
    void setNear(float nearZ) { m_nearZ = nearZ; }

    //! ファークリップ距離の取得
    float getFar() const { return m_farZ; }

    //! ファークリップ距離の設定
    void setFar(float farZ) { m_farZ = farZ; }

    //! カメラ優先度の取得（高いほど優先される）
    int getDepth() const { return m_depth; }

    //! カメラ優先度の設定
    void setDepth(int depth) { m_depth = depth; }

    // ─── 行列 ────────────────────────────────────────────

    //! ビュー行列の取得
    Matrix getView() const;

    //! プロジェクション行列の取得
    Matrix getProjection() const;

    // ─── 位置・方向ヘルパー ──────────────────────────────

    //! カメラのワールド座標
    Vector3 getPosition() const;

    //! カメラの前方ベクトル
    Vector3 getForward() const;

    //! カメラの右ベクトル
    Vector3 getRight() const;

    //! カメラの上ベクトル
    Vector3 getUp() const;

    //! カメラの回転（クォータニオン）
    Quaternion getRotation() const;

    // ─── GPU 定数バッファ ────────────────────────────────

    //! GPU 上の定数バッファアドレスの取得
    D3D12_GPU_VIRTUAL_ADDRESS getGPUAddress() const { return m_cameraCB->getGPUAddress(); }

    //! 定数バッファを GPU へアップロード
    void uploadToGPU();

protected:

    //! 同 GameObject の TransformComponent（サブクラスからも参照可能）
    TransformComponent* m_transform = nullptr;

private:

    //! CameraManager への登録（start/onEnable で呼ぶ）
    void registerToManager();

    //! CameraManager からの解除（onDisable/onDestroy で呼ぶ）
    void unregisterFromManager();

    //! GPU に送る定数バッファ構造体
    struct GPUCameraBuffer
    {
        Matrix view;            //!< ビュー行列
        Matrix projection;      //!< プロジェクション行列
        Matrix viewProjection;  //!< ビュー×プロジェクション行列
        Vector3 cameraPos;      //!< カメラ座標
        float padding;          //!< パディング
    };

    float m_fov    = DirectX::XM_PIDIV4; //!< 視野角（ラジアン）
    float m_nearZ  = 0.1f;               //!< ニアクリップ距離
    float m_farZ   = 1000.0f;            //!< ファークリップ距離
    int   m_depth  = 0;                  //!< カメラ優先度

    bool m_registered = false; //!< CameraManager 登録済みフラグ

    std::unique_ptr<ConstantBuffer<GPUCameraBuffer>> m_cameraCB; //!< GPU 定数バッファ
};
