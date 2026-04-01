#pragma once

#include "Component\Component.h"

class TransformComponent;

//=====================================================
// フリーカメラコンポーネント
// - WASD でカメラを移動、右マウスドラッグで視点回転
// - Shift 押しっぱなしで高速移動
// - マウスホイールで前後移動
// - 同じ GameObject に TransformComponent が必要
//=====================================================
class FreeCameraComponent : public Component
{
public:

    FreeCameraComponent() = default;
    ~FreeCameraComponent() override = default;

    //! start で TransformComponent をキャッシュし初期回転を設定
    void start() override;

    //! 毎フレーム入力処理
    void update() override;

    //! インスペクタ表示
    void inspectGUI() override;

private:

    float m_yaw             = DirectX::XMConvertToRadians(180.0f); //!< 水平角（左右）
    float m_pitch           = 0.0f;                                //!< 垂直角（上下）
    float m_moveSpeed       = 8.0f;                                //!< 移動速度
    float m_mouseSensitivity = 0.0025f;                            //!< マウス感度

    TransformComponent* m_transform = nullptr; //!< 同 GameObject の Transform（キャッシュ）
};
