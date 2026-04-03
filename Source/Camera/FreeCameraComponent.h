#pragma once

#include "Camera\CameraComponent.h"

//=====================================================
// フリーカメラコンポーネント
//
// CameraComponent を継承した具体的なカメラ挙動。
// 1 つの addComponent<FreeCameraComponent>() だけで
// カメラ機能（プロジェクション・ビュー行列・CameraManager 登録）
// と入力操作（WASD 移動・マウス回転・ホイールズーム）が
// すべて有効になる。
//
// ■ 必要なコンポーネント
//   - TransformComponent（位置・回転の管理）
//
// ■ 操作
//   - WASD    : 前後左右移動
//   - E / Q   : 上昇 / 下降
//   - 右ドラッグ : 視点回転
//   - Shift   : 高速移動
//   - ホイール : 前後移動
//=====================================================
class FreeCameraComponent : public CameraComponent
{
public:

    FreeCameraComponent() = default;
    ~FreeCameraComponent() override = default;

    //! start で TransformComponent をキャッシュし初期回転を設定
    void start() override;

    //! 毎フレーム入力処理
    void update() override;

    //! インスペクタ表示（カメラプロパティ＋フリーカメラ設定）
    void inspectGUI() override;

private:

    float m_yaw              = DirectX::XMConvertToRadians(180.0f); //!< 水平角（左右）
    float m_pitch            = 0.0f;                                //!< 垂直角（上下）
    float m_moveSpeed        = 8.0f;                                //!< 移動速度
    float m_mouseSensitivity = 0.0025f;                             //!< マウス感度
};
