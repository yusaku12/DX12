#pragma once

#include "Scene.h"
#include "Test\TestPolygon.h"

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

private:

    std::unique_ptr<TestPolygon> m_testPolygon;
};