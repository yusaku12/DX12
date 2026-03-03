#pragma once

#include "Scene.h"
#include "Render\PMXRender.h"
#include "Render\FBXRender.h"

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

    void debugDraw() override;

private:

    std::unique_ptr<PMXRender> m_pmxRender;
    std::unique_ptr<FBXRender> m_fbxRender;
};