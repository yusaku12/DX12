#pragma once

#include "Scene.h"
#include <chrono>

//============================================================
// テスト用シーン
//============================================================
class TestScene : public Scene
{
public:

    void onEnter() override;

    void onExit() override;

    void update() override;

    void draw() override;

    void drawMultiThreaded() override;

    void debugDraw() override;
};