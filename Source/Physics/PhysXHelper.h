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
    inline physx::PxVec3 toPx(const Vector3& v)
    {
        return physx::PxVec3(v.x, v.y, v.z);
    }

    //! PxVec3 -> Vector3
    inline Vector3 toSM(const physx::PxVec3& v)
    {
        return Vector3(v.x, v.y, v.z);
    }

    //! Quaternion -> PxQuat
    inline physx::PxQuat toPx(const Quaternion& q)
    {
        return physx::PxQuat(q.x, q.y, q.z, q.w);
    }

    //! PxQuat -> Quaternion
    inline Quaternion toSM(const physx::PxQuat& q)
    {
        return Quaternion(q.x, q.y, q.z, q.w);
    }

    //! (Position, Rotation) -> PxTransform
    inline physx::PxTransform toPx(const Vector3& pos, const Quaternion& rot)
    {
        return physx::PxTransform(toPx(pos), toPx(rot));
    }

    //! PxTransform -> (Position, Rotation)
    inline void toSM(const physx::PxTransform& t, Vector3& outPos, Quaternion& outRot)
    {
        outPos = toSM(t.p);
        outRot = toSM(t.q);
    }
}