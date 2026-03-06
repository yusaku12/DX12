#pragma once

#include <PxPhysicsAPI.h>
#include <Math\SimpleMath.h>

//=====================================================
// SimpleMath <-> PhysX 型変換ヘルパー
//=====================================================
namespace PhysXHelper
{
    using namespace DirectX::SimpleMath;

    //! Vector3 -> PxVec3
    inline physx::PxVec3 ToPxVec3(const Vector3& v)
    {
        return physx::PxVec3(v.x, v.y, v.z);
    }

    //! PxVec3 -> Vector3
    inline Vector3 ToVector3(const physx::PxVec3& v)
    {
        return Vector3(v.x, v.y, v.z);
    }

    //! Vector4 -> PxVec4
    inline physx::PxVec4 ToPxVec4(const Vector4& v)
    {
        return physx::PxVec4(v.x, v.y, v.z, v.w);
    }

    //! PxVec4 -> Vector4
    inline Vector4 ToVector4(const physx::PxVec4& v)
    {
        return Vector4(v.x, v.y, v.z, v.w);
    }

    //! Quaternion -> PxQuat
    inline physx::PxQuat ToPxQuat(const Quaternion& q)
    {
        return physx::PxQuat(q.x, q.y, q.z, q.w);
    }

    //! PxQuat -> Quaternion
    inline Quaternion ToQuaternion(const physx::PxQuat& q)
    {
        return Quaternion(q.x, q.y, q.z, q.w);
    }

    //! (Position, Rotation) -> PxTransform
    inline physx::PxTransform ToPxTransform(const Vector3& pos, const Quaternion& rot)
    {
        return physx::PxTransform(ToPxVec3(pos), ToPxQuat(rot));
    }

    //! (Position, EulerAngles) -> PxTransform
    inline physx::PxTransform ToPxTransform(const Vector3& pos, const Vector3& euler)
    {
        Quaternion q = Quaternion::CreateFromYawPitchRoll(euler.y, euler.x, euler.z);
        return physx::PxTransform(ToPxVec3(pos), ToPxQuat(q));
    }

    //! Matrix -> PxMat44
    inline physx::PxMat44 ToPxMat44(const Matrix& m)
    {
        return physx::PxMat44(
            physx::PxVec4(m._11, m._12, m._13, m._14),
            physx::PxVec4(m._21, m._22, m._23, m._24),
            physx::PxVec4(m._31, m._32, m._33, m._34),
            physx::PxVec4(m._41, m._42, m._43, m._44)
        );
    }

    //! PxMat44 -> Matrix
    inline Matrix ToMatrix(const physx::PxMat44& m)
    {
        return Matrix(
            m.column0.x, m.column0.y, m.column0.z, m.column0.w,
            m.column1.x, m.column1.y, m.column1.z, m.column1.w,
            m.column2.x, m.column2.y, m.column2.z, m.column2.w,
            m.column3.x, m.column3.y, m.column3.z, m.column3.w
        );
    }
}