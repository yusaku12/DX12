#pragma once

class Scene
{
public:

private:

    //! シーンデータ(GPUで送る)
    struct SceneData
    {
        Matrix view;        //!< ビュー行列
        Matrix projection;  //!< プロジェクト行列
        Vector3 eye;        //!< カメラ位置
    };
};