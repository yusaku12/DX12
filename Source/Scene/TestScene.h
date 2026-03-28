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

    void update() override;

    void debugDraw() override;
};