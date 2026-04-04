#include "pch.h"
#include "ObjectPicker.h"
#include "Editor/EditorContext.h"
#include "GameObject\GameObject.h"
#include "Physics\Raycast.h"
#include "Component\TransformComponent.h"
#include "Component\FbxRenderComponent.h"
#include "Camera\CameraComponent.h"

void ObjectPicker::update()
{
    if (!m_enabled) return;

    // 左クリックされたフレームのみ処理
    if (!InputManager::Instance().isMousePressed(0)) return;

    // Scene ウィンドウのサイズが有効でなければスキップ
    ImVec2 sceneSize = DX12::Instance().getSceneWindowSize();
    if (sceneSize.x <= 0.0f || sceneSize.y <= 0.0f) return;

    // マウス座標をレンダリングビューポート座標に変換
    float vpX, vpY;
    if (!getViewportCoord(vpX, vpY)) return;

    auto* cam = CameraManager::Instance().getMainCamera();
    if (!cam) return;

    // Scene ウィンドウのアスペクト比を使ったプロジェクション行列
    float sceneAspect = sceneSize.x / sceneSize.y;
    Matrix projection = Matrix::CreatePerspectiveFieldOfView(
        cam->getFov(), sceneAspect, cam->getNear(), cam->getFar());

    // Sceneウィンドウサイズをビューポートとしてレイを生成
    Ray ray = Physics::ScreenPointToRay(
        vpX, vpY,
        0.0f, 0.0f,
        sceneSize.x, sceneSize.y,
        cam->getView(), projection);

    float maxDistance = cam->getFar();

    // PhysX レイキャスト（Rigidbody + Collider 付きオブジェクト）
    Physics::RaycastHit physxHit;
    if (Physics::Raycast(ray, maxDistance, physxHit) && physxHit.gameObject)
    {
        g_editor.selectedObject = physxHit.gameObject;
        return;
    }

    // 全 GameObject の AABB とレイ交差判定（フォールバック）
    GameObject* picked = pickByAABB(ray, maxDistance);
    g_editor.selectedObject = picked; // nullptr なら選択解除
}

bool ObjectPicker::getViewportCoord(float& outVpX, float& outVpY) const
{
    // GetCursorPos はデスクトップ座標 → ScreenToClient で HWND クライアント座標に変換
    POINT cursorPos = InputManager::Instance().getMousePosition();
    ScreenToClient(DX12::Instance().getHwnd(), &cursorPos);

    float mx = static_cast<float>(cursorPos.x);
    float my = static_cast<float>(cursorPos.y);

    // Scene ウィンドウの矩形（ImGui クライアント座標系）
    ImVec2 scenePos = DX12::Instance().getSceneWindowPos();
    ImVec2 sceneSize = DX12::Instance().getSceneWindowSize();

    // Scene ウィンドウ内かチェック
    if (mx < scenePos.x || mx > scenePos.x + sceneSize.x) return false;
    if (my < scenePos.y || my > scenePos.y + sceneSize.y) return false;

    // Sceneウィンドウ内のローカル座標をそのまま返す
    outVpX = mx - scenePos.x;
    outVpY = my - scenePos.y;
    return true;
}

GameObject* ObjectPicker::pickByAABB(const Ray& ray, float maxDistance) const
{
    using namespace DirectX;

    GameObject* closestObj = g_editor.selectedObject;
    float closestDist = maxDistance;

    for (GameObject* obj : GameObjectRegistry::Instance().getAll())
    {
        if (!obj || obj->isDestroyed() || !obj->isEnabled()) continue;

        auto* fbx = obj->getComponent<FbxRenderComponent>();
        if (!fbx) continue;

        Vector3 center, extents;
        if (!fbx->getWorldAABB(center, extents)) continue;

        BoundingBox aabb;
        aabb.Center = XMFLOAT3(center.x, center.y, center.z);
        aabb.Extents = XMFLOAT3(extents.x, extents.y, extents.z);

        float dist = 0.0f;
        if (ray.Intersects(aabb, dist))
        {
            if (dist >= 0.0f && dist < closestDist)
            {
                closestDist = dist;
                closestObj = obj;
            }
        }
    }

    return closestObj;
}