#include "pch.h"
#include "ModelEditorScene.h"
#include "Component\TransformComponent.h"
#include "Component\FbxRenderComponent.h"
#include "Camera\FreeCameraComponent.h"

void ModelEditorScene::onEnter()
{
    // カメラオブジェクトを作成
    // FreeCameraComponent は CameraComponent を継承しているため
    // これ1つを addComponent するだけでカメラ機能＋自由視点操作が有効になる
    GameObject* cameraObject = new GameObject("MainCamera");
    cameraObject->addComponent<TransformComponent>()->setPosition({ 0.0f, 9.0f, -23.0f });
    cameraObject->addComponent<FreeCameraComponent>();

    GameObject* object = new GameObject("ModelObject");
    object->addComponent<TransformComponent>();
    object->addComponent<FbxRenderComponent>("Data/Model/Jammo/Jammo.fbx");
}

void ModelEditorScene::update()
{
    // グリッドは毎フレーム描画リクエストを出す
    DebugPrimitive::Instance().drawGrid({ 0.0f,0.0f,0.0f }, 20.0f, 20.0f, 1.0f, { 0.5f,0.5f,0.5f,1.0f });
}

void ModelEditorScene::debugDraw()
{
}

void ModelEditorScene::openFbxFile()
{
}