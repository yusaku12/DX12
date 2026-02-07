#pragma once
#include "Math\SimpleMath.h"

using namespace DirectX::SimpleMath;
//=====================================================
// カメラクラス
//=====================================================
class Camera
{
public:

    //! ビュー行列の取得
    Matrix getView() const
    {
        Vector3 forward = Vector3::Transform(Vector3::Forward, m_rotation);
        Vector3 up = Vector3::Transform(Vector3::Up, m_rotation);
        return Matrix::CreateLookAt(m_position, m_position + forward, up);
    }

    //! プロジェクション行列の取得
    Matrix getProjection() const
    {
        return Matrix::CreatePerspectiveFieldOfView(m_fov, m_aspect, m_nearZ, m_farZ);
    }

    // カメラ座標取得
    const Vector3& getPosition() const { return m_position; }

private:

    Vector3 m_position = { 0,0,0 };                //!< カメラの位置
    Quaternion m_rotation = Quaternion::Identity;  //!< カメラの回転(クォータニオン)

    float m_fov = DirectX::XMConvertToRadians(30.0f); //!< 視野角
    float m_aspect = static_cast<float>(DX12::Instance().getScreenWidth()) / static_cast<float>(DX12::Instance().getScreenHeight()); //!< アスペクト比
    float m_nearZ = 0.1f;   //!< ニアクリップ距離
    float m_farZ = 1000.0f; //!< ファークリップ距離
};