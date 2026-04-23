#include "pch.h"
#include "ModelEditorScene.h"
#include "Component\TransformComponent.h"
#include "Component\FbxRenderComponent.h"
#include "Camera\FreeCameraComponent.h"
#include "Component\AnimationComponent.h"
#include "Component\PostEffectComponent.h"
#include "PostEffect\BloomEffect.h"
#include "PostEffect\ColorGradingEffect.h"

void ModelEditorScene::onEnter()
{
    GameObject* cameraObject = new GameObject("MainCamera");
    cameraObject->addComponent<TransformComponent>()->setPosition({ 0.0f, 9.0f, -23.0f });
    cameraObject->addComponent<FreeCameraComponent>();

    GameObject* object = new GameObject("ModelObject");
    object->addComponent<TransformComponent>();
    object->addComponent<FbxRenderComponent>("Data/Model/Jammo/Jammo.fbx");
    object->addComponent<AnimationComponent>();

    GameObject* postEffectObj = new GameObject("PostEffectVolume");
    postEffectObj->addTag(Tag::PostEffect);
    auto* pe = postEffectObj->addComponent<PostEffectComponent>();
    pe->addEffect<BloomEffect>();
    pe->addEffect<ColorGradingEffect>();
}

void ModelEditorScene::update()
{
    // グリッドは毎フレーム描画リクエストを出す
    DebugPrimitive::Instance().drawGrid({ 0.0f,0.0f,0.0f }, 40.0f, 40.0f, 1.0f, { 0.5f,0.5f,0.5f,1.0f });
}

void ModelEditorScene::debugDraw()
{
}

void ModelEditorScene::openFbxFile()
{
}