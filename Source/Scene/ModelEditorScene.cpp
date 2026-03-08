#include "pch.h"
#include "ModelEditorScene.h"

void ModelEditorScene::onEnter()
{
}

void ModelEditorScene::onExit()
{
}

void ModelEditorScene::update()
{
    //! グリッドは毎フレーム描画リクエストを出す
    DebugPrimitive::Instance().drawGrid({ 0.0f,0.0f,0.0f }, 100.0f, 100.0f, 1.0f, { 0.5f,0.5f,0.5f,1.0f });
}

void ModelEditorScene::draw()
{
}

void ModelEditorScene::drawMultiThreaded()
{
}

void ModelEditorScene::debugDraw()
{
}