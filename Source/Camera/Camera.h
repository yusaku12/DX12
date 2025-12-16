#pragma once
#include "pch.h"

//=====================================================
// カメラクラス
//=====================================================
class Camera
{
public:

    Vector3 m_position{ 0,0,0 };    //!< カメラの位置
    Vector4 m_rotation{ 0,0,0,1 };  //!< カメラの回転(クォータニオン)

    float m_fov = DirectX::XM_PIDIV4; //!< 視野角
    float m_aspect = 16.0f / 9.0f;    //!< アスペクト比
    float nearZ = 0.1f;               //!< ニアクリップ距離
    float farZ = 1000.0f;             //!< ファークリップ距離

    //XMMATRIX GetView() const
    //{
    //    XMVECTOR pos = XMLoadFloat3(&position);
    //    XMVECTOR rot = XMLoadFloat4(&rotation);

    //    XMVECTOR forward = XMVector3Rotate({ 0,0,1,0 }, rot);
    //    XMVECTOR up = XMVector3Rotate({ 0,1,0,0 }, rot);

    //    return XMMatrixLookToLH(pos, forward, up);
    //}

    //XMMATRIX GetProj() const
    //{
    //    return XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);
    //}
};