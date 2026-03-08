#pragma once

#include "Scene.h"
#include <chrono>

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
};