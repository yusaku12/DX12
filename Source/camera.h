#pragma once

//=====================================================
// Camera クラス
//=====================================================
class Camera
{
public:

    //! シングルトンインスタンス取得
    static Camera& Instance()
    {
        static Camera instance;
        return instance;
    }

    //! 更新処理
    void update();

    //! デバッグ表示
    void debug();

    //! クリップ空間行列作成
    void createClipMatrix();

    //! ビュー行列取得
    const Matrix& getMatrix()const { return m_matrix; }

private:

    Camera() = default;
    ~Camera() = default;

    float m_near = 1.0f;
    float m_far = 10.0f;
    Vector3 m_eye = { 0,0,-5 };
    Vector3 m_target = { 0,0,0 };
    Vector3 m_up = { 0,1,0 };
    Matrix m_matrix = Matrix::Identity;
    Matrix m_worldMatrix = Matrix::Identity;
};