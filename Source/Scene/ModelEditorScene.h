#pragma once

#include "Scene.h"
#include "Render\FbxRender.h"
#include "Component\TransformComponent.h"

//============================================================
// モデルエディタシーン
//============================================================
class ModelEditorScene : public Scene
{
public:

    void onEnter() override;

    void onExit() override;

    void update() override;

    void draw() override;

    void drawMultiThreaded() override;

    void debugDraw() override;

private:

    //! ファイルダイアログから FBX を読み込む
    void openFbxFile();

    std::unique_ptr<FbxRender> m_fbxRender;
    TransformComponent* m_transform;
};