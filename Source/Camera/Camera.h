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
        return Matrix::CreatePerspectiveFieldOfView(
            m_fov,
            m_aspect,
            m_nearZ,
            m_farZ
        );
    }

    //! カメラ座標取得
    Vector3& getPosition() { return m_position; }

    //! カメラ座標設定
    void setPosition(const Vector3& pos) { m_position = pos; }

    //! カメラ回転取得
    Quaternion& getRotation() { return m_rotation; }

    //! カメラ回転設定
    void setRotation(const Quaternion& rot) { m_rotation = rot; }

    //! 前ベクトル
    Vector3 getForward() const { return Vector3::Transform(Vector3::Forward, m_rotation); }

    //! 右ベクトル
    Vector3 getRight() const { return Vector3::Transform(Vector3::Right, m_rotation); }

    //! 上ベクトル
    Vector3 getUp() const { return Vector3::Transform(Vector3::Up, m_rotation); }

    //! 視野角の取得
    float& getFov() { return m_fov; }

    //! ニアクリップの取得
    float& getNear() { return m_nearZ; }

    //! ファークリップ距離の取得
    float& getFar() { return m_farZ; }

private:

    Vector3 m_position = { 0.0f, 9.0f, -23.0f };   //!< カメラの位置
    Quaternion m_rotation = Quaternion::Identity;  //!< カメラの回転(クォータニオン)

    float m_fov = DirectX::XM_PIDIV4; //!< 視野角
    float m_aspect = static_cast<float>(DX12::Instance().getScreenWidth()) / static_cast<float>(DX12::Instance().getScreenHeight()); //!< アスペクト比
    float m_nearZ = 0.1f;   //!< ニアクリップ距離
    float m_farZ = 1000.0f; //!< ファークリップ距離
};