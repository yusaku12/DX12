#pragma once

#include "Component\Component.h"
#include "GameObject\GameObject.h"

//=====================================================
// Transform コンポーネント
// - 位置 / 回転(Quaternion) / スケール を管理
// - ローカル行列 / ワールド行列を取得（親の Transform があれば考慮）
// - Inspector GUI と Gizmo 更新を分離して、Inspector を閉じても Gizmo が動作するようにする
//=====================================================
class TransformComponent : public Component
{
public:

    TransformComponent() = default;
    ~TransformComponent() override = default;

    //! 生成直後に一度だけ呼ばれる
    void awake() override {}

    //! ゲーム開始時に一度だけ呼ばれる
    void start() override {}

    //! インスペクタ表示
    void inspectGUI() override;

    //! Gizmo の更新（Inspector が閉じていても毎フレーム実行される）
    void onGizmo();

    //! 位置
    Vector3& getPosition() { return m_position; }
    void setPosition(const Vector3& pos) { m_position = pos; m_dirty = true; }

    //! 回転（クォータニオン）
    Quaternion& getRotation() { return m_rotation; }
    void setRotation(const Quaternion& rot) { m_rotation = rot; m_dirty = true; }

    //! スケール
    Vector3& getScale() { return m_scale; }
    void setScale(const Vector3& scale) { m_scale = scale; m_dirty = true; }

    //! 相対移動
    void translate(const Vector3& delta) { m_position += delta; m_dirty = true; }

    //! クォータニオンで回転（引数のクォータニオンを現在回転の前に適用）
    void rotate(const Quaternion& q)
    {
        m_rotation = q * m_rotation;
        m_rotation.Normalize();
        m_dirty = true;
    }

    //! 軸・角度(ラジアン) で回転
    void rotateAxisAngle(const Vector3& axis, float angleRad) { rotate(Quaternion::CreateFromAxisAngle(axis, angleRad)); }

    //! ローカル行列（スケール→回転→並進）
    Matrix getLocalMatrix() const;

    //! ワールド行列（親の Transform があれば親のワールド行列を考慮）
    Matrix getWorldMatrix() const;

    //! 外部から dirty を立てる（親変更などで使用）
    void markDirty() { m_dirty = true; }

private:

    Vector3 m_position = Vector3::Zero;
    Quaternion m_rotation = Quaternion::Identity;
    Vector3 m_scale = Vector3::One;

    //! ギズモモード（0=Translate,1=Rotate,2=Scale）を保持しておく
    int m_gizmoOp = 0;

    //! キャッシュ
    mutable Matrix m_localMatrix = Matrix::Identity;
    mutable Matrix m_worldMatrix = Matrix::Identity;
    mutable bool m_dirty = true;
};