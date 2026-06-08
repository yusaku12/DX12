#pragma once

#include "Scene.h"

//============================================================
// パーティクルシーン
//============================================================
class ParticleScene : public Scene
{
public:

    void onEnter() override;

    void update() override;

    void debugDraw() override;
};