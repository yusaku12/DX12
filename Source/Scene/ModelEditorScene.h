#pragma once

#include "Scene.h"

//============================================================
// モデルエディタシーン
//============================================================
class ModelEditorScene : public Scene
{
public:

    void onEnter() override;

    void update() override;

    void debugDraw() override;

private:

    //! ファイルダイアログから FBX を読み込む
    void openFbxFile();
};