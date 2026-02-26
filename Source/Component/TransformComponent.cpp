#include "pch.h"
#include "TransformComponent.h"

Matrix TransformComponent::getLocalMatrix() const
{
    if (!m_dirty) return m_localMatrix;

    //! スケール → 回転(クォータニオン) → 並進
    m_localMatrix = Matrix::CreateScale(m_scale)
        * Matrix::CreateFromQuaternion(m_rotation)
        * Matrix::CreateTranslation(m_position);

    m_dirty = false;
    return m_localMatrix;
}

Matrix TransformComponent::getWorldMatrix() const
{
    //! ローカル行列を取得（必要なら再計算）
    Matrix local = getLocalMatrix();

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

    //! 親のワールド行列を先に適用（parent * local）
    m_worldMatrix = parentTf->getWorldMatrix() * local;
    return m_worldMatrix;
}

void TransformComponent::onInspectorGUI()
{
    //! Position
    {
        float pos[3] = { m_position.x, m_position.y, m_position.z };
        if (ImGui::DragFloat3("Position", pos, 0.1f))
        {
            m_position = Vector3(pos[0], pos[1], pos[2]);
            m_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Pos"))
        {
            m_position = Vector3::Zero;
            m_dirty = true;
        }
    }

    ImGui::Separator();

    //! Rotation (表示は度、内部はクォータニオン)
    {
        //! Quaternion -> Euler (ラジアン) -> 度に変換して表示
        Vector3 eulerRad = m_rotation.ToEuler(); // ラジアン想定
        float rotDeg[3] = {
            DirectX::XMConvertToDegrees(eulerRad.x),
            DirectX::XMConvertToDegrees(eulerRad.y),
            DirectX::XMConvertToDegrees(eulerRad.z)
        };

        if (ImGui::DragFloat3("Rotation (deg)", rotDeg, 1.0f))
        {
            //! UIでは (X=pitch, Y=yaw, Z=roll) として扱う
            float pitch = DirectX::XMConvertToRadians(rotDeg[0]);
            float yaw = DirectX::XMConvertToRadians(rotDeg[1]);
            float roll = DirectX::XMConvertToRadians(rotDeg[2]);

            //! CreateFromYawPitchRoll のシグネチャは (yaw, pitch, roll)
            m_rotation = Quaternion::CreateFromYawPitchRoll(yaw, pitch, roll);
            m_dirty = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset Rot"))
        {
            m_rotation = Quaternion::Identity;
            m_dirty = true;
        }
    }

    ImGui::Separator();

    //! Scale
    {
        float scale[3] = { m_scale.x, m_scale.y, m_scale.z };
        if (ImGui::DragFloat3("Scale", scale, 0.01f, 0.001f))
        {
            m_scale = Vector3(scale[0], scale[1], scale[2]);
            m_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Scale"))
        {
            m_scale = Vector3::One;
            m_dirty = true;
        }
    }
}