#include "pch.h"
#include "Camera/CameraManager.h"
#include "Raycast.h"
#include "PhysXHelper.h"
#include "Component\RigidbodyComponent.h"
#include "Component\TransformComponent.h"
#include "GameObject\GameObject.h"
#include "Camera\CameraComponent.h"

namespace Physics
{
    static RaycastHit Convert(const PhysicsWorld::RaycastHit& src)
    {
        RaycastHit dst;
        dst.point = src.point;
        dst.normal = src.normal;
        dst.distance = src.distance;
        dst.rigidbody = src.rigidbody;
        dst.gameObject = src.rigidbody ? src.rigidbody->gameObject() : nullptr;
        return dst;
    }

    bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit)
    {
        Vector3 dir = direction;
        dir.Normalize();

        PhysicsWorld::RaycastHit pwHit;
        if (PhysicsWorld::Instance().raycast(origin, dir, maxDistance, pwHit))
        {
            outHit = Convert(pwHit);
            return true;
        }
        return false;
    }

    bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance)
    {
        RaycastHit hit;
        return Raycast(origin, direction, maxDistance, hit);
    }

    bool Raycast(const Ray& ray, float maxDistance, RaycastHit& outHit)
    {
        return Raycast(ray.position, ray.direction, maxDistance, outHit);
    }

    bool RaycastAll(const Vector3& origin, const Vector3& direction, float maxDistance, std::vector<RaycastHit>& outHits)
    {
        Vector3 dir = direction;
        dir.Normalize();

        std::vector<PhysicsWorld::RaycastHit> pwHits;
        if (PhysicsWorld::Instance().raycastAll(origin, dir, maxDistance, pwHits))
        {
            outHits.clear();
            outHits.reserve(pwHits.size());
            for (const auto& h : pwHits)
            {
                outHits.push_back(Convert(h));
            }
            return true;
        }
        return false;
    }

    bool RaycastAll(const Ray& ray, float maxDistance, std::vector<RaycastHit>& outHits)
    {
        return RaycastAll(ray.position, ray.direction, maxDistance, outHits);
    }

    Ray ScreenPointToRay(
        float screenX, float screenY,
        float viewportX, float viewportY,
        float viewportW, float viewportH,
        const Matrix& view, const Matrix& projection)
    {
        // ビューポートは (0,0) 始まりで構築（渡される座標もこの空間に合わせる）
        Viewport vp(viewportX, viewportY, viewportW, viewportH);

        // Near 平面上の点（z=0）
        Vector3 nearPoint = vp.Unproject(
            Vector3(screenX, screenY, 0.0f),
            projection, view, Matrix::Identity);

        // Far 平面上の点（z=1）
        Vector3 farPoint = vp.Unproject(
            Vector3(screenX, screenY, 1.0f),
            projection, view, Matrix::Identity);

        Vector3 dir = farPoint - nearPoint;
        dir.Normalize();

        return Ray(nearPoint, dir);
    }

    Ray ScreenPointToRay(float screenX, float screenY)
    {
        auto* cam = CameraManager::Instance().getMainCamera();
        if (!cam)
        {
            return Ray(Vector3::Zero, Vector3::Forward);
        }

        // GPU レンダリングビューポート（(0,0) 始まり、レンダーターゲットサイズ）
        float w = static_cast<float>(DX12::Instance().getScreenWidth());
        float h = static_cast<float>(DX12::Instance().getScreenHeight());

        return ScreenPointToRay(
            screenX, screenY,
            0.0f, 0.0f,
            w, h,
            cam->getView(), cam->getProjection());
    }
}