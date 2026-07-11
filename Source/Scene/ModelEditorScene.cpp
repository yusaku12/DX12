#include "pch.h"
#include "ModelEditorScene.h"
#include "Component\TransformComponent.h"
#include "Component\FbxRenderComponent.h"
#include "Camera\FreeCameraComponent.h"
#include "Component\AnimationComponent.h"
#include "Component\PostEffectComponent.h"
#include "PostEffect\BloomEffect.h"
#include "PostEffect\CasSharpenEffect.h"
#include "PostEffect\ColorGradingEffect.h"
#include "PostEffect\DepthOfFieldEffect.h"
#include "PostEffect\MotionBlurEffect.h"
#include "PostEffect\TemporalAAEffect.h"
#include "Component\SkyboxComponent.h"
#include "Component\GpuEffectComponent.h"
#include "UISetupTemplate.h"

void ModelEditorScene::onEnter()
{
    GameObject* cameraObject = DX_NEW(GameObject, "MainCamera");
    cameraObject->addComponent<TransformComponent>()->setPosition({ 0.0f, 9.0f, -23.0f });
    cameraObject->addComponent<FreeCameraComponent>();

    GameObject* object = DX_NEW(GameObject, "ModelObject");
    object->addComponent<TransformComponent>();
    object->addComponent<FbxRenderComponent>("Data/Model/Jammo/Jammo.mdl");
    object->addComponent<AnimationComponent>();

    GameObject* skyboxObj = DX_NEW(GameObject, "Skybox");
    auto* skybox = skyboxObj->addComponent<SkyboxComponent>();
    skybox->setCubemap(L"Data/Texture/test.dds");

    GameObject* postEffectObj = DX_NEW(GameObject, "PostEffectVolume");
    postEffectObj->addComponent<TransformComponent>();
    postEffectObj->addTag(Tag::PostEffect);
    auto* pe = postEffectObj->addComponent<PostEffectComponent>();
    pe->addEffect<BloomEffect>();
    //pe->addEffect<DepthOfFieldEffect>();
    pe->addEffect<MotionBlurEffect>();
    pe->addEffect<TemporalAAEffect>();
    pe->addEffect<ColorGradingEffect>();
    pe->addEffect<CasSharpenEffect>();
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