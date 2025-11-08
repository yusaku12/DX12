#include "pch.h"
#include "camera.h"

Camera& Camera::Instance()
{
    static Camera instance;
    return instance;
}

void Camera::update()
{
}

void Camera::debug()
{
}

void Camera::createClipMatrix()
{
    //! ビュー行列（カメラ視点）
    Matrix lookMatrix = DirectX::XMMatrixLookAtLH(m_eye, m_target, m_up);

    //! 投影行列（遠近感）
    Matrix projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, static_cast<float>(DX12::getInstance().getScreenWidth()) / static_cast<float>(DX12::getInstance().getScreenHeight()), m_near, m_far);

    //! 合成
    m_matrix *= m_worldMatrix;
    m_matrix *= lookMatrix;
    m_matrix *= projectionMatrix;
}