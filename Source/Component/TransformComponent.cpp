#include "pch.h"
#include "TransformComponent.h"
#include "ImGuizmo.h"
#include "RigidbodyComponent.h"

const Matrix& TransformComponent::getLocalMatrix() const
{
    if (!m_dirty) return m_localMatrix;

    //! スケール → 回転(クォータニオン) → 並進
    m_localMatrix = Matrix::CreateScale(m_scale)
        * Matrix::CreateFromQuaternion(m_rotation)
        * Matrix::CreateTranslation(m_position);

    m_dirty = false;
    return m_localMatrix;
}

const Matrix& TransformComponent::getWorldMatrix() const
{
    //! ローカル行列を取得（必要なら再計算）
    const Matrix& local = getLocalMatrix();

    //! 親がいない場合はローカルがワールド
    GameObject* parent = nullptr;
    if (gameObject()) parent = gameObject()->getParent();

    if (!parent)
    {
        m_worldMatrix = local;
        return m_worldMatrix;
    }

    //! 親に TransformComponent があれば親のワールド行列を取得して合成
    TransformComponent* parentTf = parent->getComponent<TransformComponent>();
    if (!parentTf)
    {
        m_worldMatrix = local;
        return m_worldMatrix;
    }

    //! ローカル × 親ワールド
    m_worldMatrix = local * parentTf->getWorldMatrix();
    return m_worldMatrix;
}

void TransformComponent::inspectGUI()
{
    //! Reset ボタンのみ（Inspector 内の UI）
    if (ImGui::Button("Reset"))
    {
        m_position = Vector3::Zero;
        m_rotation = Quaternion::Identity;
        m_scale = Vector3::One;
        m_dirty = true;
    }

    ImGui::SameLine();

    //! ギズモ操作モード切替（Inspector が閉じても m_gizmoOp を保持しておく）
    if (ImGui::RadioButton("T", m_gizmoOp == 0)) m_gizmoOp = 0; ImGui::SameLine();
    if (ImGui::RadioButton("R", m_gizmoOp == 1)) m_gizmoOp = 1; ImGui::SameLine();
    if (ImGui::RadioButton("S", m_gizmoOp == 2)) m_gizmoOp = 2;
}

void TransformComponent::onGizmo()
{
    //! ImGuizmo フレーム開始（毎フレーム必須）
    ImGuizmo::BeginFrame();

    //! 現在の ImGui コンテキストを使用
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

    //! カメラ行列取得
    Matrix view = CameraManager::Instance().getView();
    Matrix proj = CameraManager::Instance().getProjection();

    //! 描画設定
    ImGuizmo::SetDrawlist(DX12::Instance().getSceneDrawList());
    ImGuizmo::SetOrthographic(false);

    //! シーン描画領域を設定（ImGuiのImage表示領域）
    const ImVec2 scenePos = DX12::Instance().getSceneWindowPos();
    const ImVec2 sceneSize = DX12::Instance().getSceneWindowSize();
    if (sceneSize.x > 1.0f && sceneSize.y > 1.0f)
    {
        ImGuizmo::SetRect(scenePos.x, scenePos.y, sceneSize.x, sceneSize.y);
    }
    else
    {
        //! フォールバック：メインビューポート全体
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGuizmo::SetRect(vp->Pos.x, vp->Pos.y, vp->Size.x, vp->Size.y);
    }

    //! 操作対象ローカル行列
    Matrix localMat = getLocalMatrix();

    //! 操作モード決定
    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    switch (m_gizmoOp)
    {
    case 1: operation = ImGuizmo::ROTATE; break;
    case 2: operation = ImGuizmo::SCALE;  break;
    default: break;
    }

    //! ギズモ操作実行
    bool manipulated = ImGuizmo::Manipulate(
        &view._11,
        &proj._11,
        operation,
        ImGuizmo::LOCAL,
        &localMat._11
    );

    //! 変更があった場合のみ Transform へ反映
    if (manipulated)
    {
        float translation[3];
        float rotationDeg[3];
        float scale[3];

        //! 行列を分解（回転は degree で返る）
        ImGuizmo::DecomposeMatrixToComponents(
            &localMat._11,
            translation,
            rotationDeg,
            scale
        );

        //! 位置更新
        m_position = Vector3(
            translation[0],
            translation[1],
            translation[2]
        );

        //! 回転更新（degree → radian → Quaternion）
        const float pitch = DirectX::XMConvertToRadians(rotationDeg[0]);
        const float yaw = DirectX::XMConvertToRadians(rotationDeg[1]);
        const float roll = DirectX::XMConvertToRadians(rotationDeg[2]);

        m_rotation = Quaternion::CreateFromYawPitchRoll(yaw, pitch, roll);

        //! スケール更新
        m_scale = Vector3(scale[0], scale[1], scale[2]);

        m_dirty = true;

        //! PhysX アクターへ位置・回転を同期し、スリープ解除する
        auto* rb = m_gameObject->getComponent<RigidbodyComponent>();
        if (rb && rb->getPxActor())
        {
            rb->syncToPhysics();
            rb->wakeUp();
        }
    }
}