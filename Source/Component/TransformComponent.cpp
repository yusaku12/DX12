#include "pch.h"
#include "GameObject/GameObjectRegistry.h"
#include "Camera/CameraManager.h"
#include "Camera/CameraComponent.h"
#include "TransformComponent.h"
#include "Editor/EditorTransaction.h"
#include "ImGuizmo.h"
#include "RigidbodyComponent.h"
#include "Camera\CameraComponent.h"

namespace
{
    bool isNearlyEqual(float a, float b, float epsilon = 0.0001f)
    {
        return std::abs(a - b) <= epsilon;
    }

    bool isEqualState(const TransformComponent::LocalState& a, const TransformComponent::LocalState& b)
    {
        return isNearlyEqual(a.position.x, b.position.x)
            && isNearlyEqual(a.position.y, b.position.y)
            && isNearlyEqual(a.position.z, b.position.z)
            && isNearlyEqual(a.rotation.x, b.rotation.x)
            && isNearlyEqual(a.rotation.y, b.rotation.y)
            && isNearlyEqual(a.rotation.z, b.rotation.z)
            && isNearlyEqual(a.rotation.w, b.rotation.w)
            && isNearlyEqual(a.scale.x, b.scale.x)
            && isNearlyEqual(a.scale.y, b.scale.y)
            && isNearlyEqual(a.scale.z, b.scale.z);
    }

    void applyStateToGameObject(uint64_t gameObjectId, const TransformComponent::LocalState& state)
    {
        GameObject* object = GameObjectRegistry::Instance().findByInstanceId(gameObjectId);
        if (!object || object->isDestroyed())
        {
            return;
        }

        auto* transform = object->getComponent<TransformComponent>();
        if (!transform)
        {
            return;
        }

        transform->applyLocalState(state);

        auto* rb = object->getComponent<RigidbodyComponent>();
        if (rb && rb->getPxActor())
        {
            rb->syncToPhysics();
        }
    }
}

const Matrix& TransformComponent::getLocalMatrix() const
{
    if (!m_dirty) return m_localMatrix;

    // スケール → 回転(クォータニオン) → 並進
    m_localMatrix = Matrix::CreateScale(m_scale)
        * Matrix::CreateFromQuaternion(m_rotation)
        * Matrix::CreateTranslation(m_position);

    m_dirty = false;
    return m_localMatrix;
}

const Matrix& TransformComponent::getWorldMatrix() const
{
    // ローカル行列を取得（必要なら再計算）
    const Matrix& local = getLocalMatrix();

    // 親がいない場合はローカルがワールド
    GameObject* parent = nullptr;
    if (gameObject()) parent = gameObject()->getParent();

    if (!parent)
    {
        m_worldMatrix = local;
        return m_worldMatrix;
    }

    // 親に TransformComponent があれば親のワールド行列を取得して合成
    TransformComponent* parentTf = parent->getComponent<TransformComponent>();
    if (!parentTf)
    {
        m_worldMatrix = local;
        return m_worldMatrix;
    }

    // ローカル × 親ワールド
    m_worldMatrix = local * parentTf->getWorldMatrix();
    return m_worldMatrix;
}

void TransformComponent::inspectGUI()
{
    // Reset ボタンのみ（Inspector 内の UI）
    if (ImGui::Button("Reset"))
    {
        const LocalState before = getLocalState();
        const LocalState after{};

        applyLocalState(after);

        if (!isEqualState(before, after) && gameObject())
        {
            const uint64_t gameObjectId = gameObject()->getInstanceId();
            EditorTransaction::Manager::Instance().record(
                "Transform Reset",
                [gameObjectId, before]() { applyStateToGameObject(gameObjectId, before); },
                [gameObjectId, after]() { applyStateToGameObject(gameObjectId, after); });
        }
    }

    ImGui::SameLine();

    // ギズモ操作モード切替（Inspector が閉じても m_gizmoOp を保持しておく）
    if (ImGui::RadioButton("T", m_gizmoOp == 0)) m_gizmoOp = 0; ImGui::SameLine();
    if (ImGui::RadioButton("R", m_gizmoOp == 1)) m_gizmoOp = 1; ImGui::SameLine();
    if (ImGui::RadioButton("S", m_gizmoOp == 2)) m_gizmoOp = 2;

    // 位置 (Position)
    ImGui::Text("postion: %.3f, %.3f, %.3f", m_position.x, m_position.y, m_position.z);

    // 回転 (Rotation) - Quaternion をオイラー角（度）に変換して表示
    {
        Vector3 eulerRad = m_rotation.ToEuler(); // ラジアンで返る
        float degX = DirectX::XMConvertToDegrees(eulerRad.x);
        float degY = DirectX::XMConvertToDegrees(eulerRad.y);
        float degZ = DirectX::XMConvertToDegrees(eulerRad.z);
        ImGui::Text("rotation: % .3f, % .3f, % .3f", degX, degY, degZ);
    }

    // スケール (Scale)
    ImGui::Text("scale: %.3f, %.3f, %.3f", m_scale.x, m_scale.y, m_scale.z);
}

void TransformComponent::onGizmo()
{
    // ImGuizmo フレーム開始（毎フレーム必須）
    ImGuizmo::BeginFrame();

    // 現在の ImGui コンテキストを使用
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

    // カメラ行列取得
    CameraComponent* camera = CameraManager::Instance().getMainCamera();
    Matrix view = camera->getView();
    Matrix proj = camera->getProjection();

    // 描画設定
    ImGuizmo::SetDrawlist(DX12::Instance().getSceneDrawList());
    ImGuizmo::SetOrthographic(false);

    // シーン描画領域を設定（ImGuiのImage表示領域）
    const ImVec2 scenePos = DX12::Instance().getSceneWindowPos();
    const ImVec2 sceneSize = DX12::Instance().getSceneWindowSize();
    if (sceneSize.x > 1.0f && sceneSize.y > 1.0f)
    {
        ImGuizmo::SetRect(scenePos.x, scenePos.y, sceneSize.x, sceneSize.y);
    }
    else
    {
        // フォールバック：メインビューポート全体
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGuizmo::SetRect(vp->Pos.x, vp->Pos.y, vp->Size.x, vp->Size.y);
    }

    // 操作対象ローカル行列
    Matrix localMat = getLocalMatrix();

    // 操作モード決定
    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    switch (m_gizmoOp)
    {
    case 1: operation = ImGuizmo::ROTATE; break;
    case 2: operation = ImGuizmo::SCALE;  break;
    default: break;
    }

    // ギズモ操作実行
    bool manipulated = ImGuizmo::Manipulate(
        &view._11,
        &proj._11,
        operation,
        ImGuizmo::LOCAL,
        &localMat._11
    );

    const bool gizmoUsing = ImGuizmo::IsUsing();
    if (gizmoUsing && !m_gizmoEditing)
    {
        m_gizmoEditing = true;
        m_gizmoBeginState = getLocalState();
    }

    // 変更があった場合のみ Transform へ反映
    if (manipulated)
    {
        float translation[3];
        float rotationDeg[3];
        float scale[3];

        // 行列を分解（回転は degree で返る）
        ImGuizmo::DecomposeMatrixToComponents(
            &localMat._11,
            translation,
            rotationDeg,
            scale
        );

        // 位置更新
        m_position = Vector3(
            translation[0],
            translation[1],
            translation[2]
        );

        // 回転更新（degree → radian → Quaternion）
        const float pitch = DirectX::XMConvertToRadians(rotationDeg[0]);
        const float yaw = DirectX::XMConvertToRadians(rotationDeg[1]);
        const float roll = DirectX::XMConvertToRadians(rotationDeg[2]);

        m_rotation = Quaternion::CreateFromYawPitchRoll(yaw, pitch, roll);

        // スケール更新
        m_scale = Vector3(scale[0], scale[1], scale[2]);

        m_dirty = true;

        // PhysX アクターへ位置・回転を同期し、スリープ解除する
        auto* rb = m_gameObject->getComponent<RigidbodyComponent>();
        if (rb && rb->getPxActor())
        {
            rb->syncToPhysics();
        }
    }

    if (!gizmoUsing && m_gizmoEditing)
    {
        m_gizmoEditing = false;

        const LocalState after = getLocalState();
        if (!isEqualState(m_gizmoBeginState, after) && gameObject())
        {
            const uint64_t gameObjectId = gameObject()->getInstanceId();
            const LocalState before = m_gizmoBeginState;

            EditorTransaction::Manager::Instance().record(
                "Transform Gizmo",
                [gameObjectId, before]() { applyStateToGameObject(gameObjectId, before); },
                [gameObjectId, after]() { applyStateToGameObject(gameObjectId, after); });
        }
    }
}

TransformComponent::LocalState TransformComponent::getLocalState() const
{
    LocalState state;
    state.position = m_position;
    state.rotation = m_rotation;
    state.scale = m_scale;
    return state;
}

void TransformComponent::applyLocalState(const LocalState& state)
{
    m_position = state.position;
    m_rotation = state.rotation;
    m_scale = state.scale;
    m_dirty = true;
}