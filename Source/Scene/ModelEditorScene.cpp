#include "pch.h"
#include "ModelEditorScene.h"
#include "Component\TransformComponent.h"
#include "Component\FbxRenderComponent.h"
#include "Camera\FreeCameraComponent.h"
#include "Component\AnimationComponent.h"
#include "Component\PostEffectComponent.h"
#include "PostEffect\BloomEffect.h"
#include "PostEffect\ColorGradingEffect.h"
#include "PostEffect\DepthOfFieldEffect.h"
#include "PostEffect\MotionBlurEffect.h"
#include "Component\SkyboxComponent.h"
#include "Component\GpuEffectComponent.h"

void ModelEditorScene::onEnter()
{
    GameObject* cameraObject = new GameObject("MainCamera");
    cameraObject->addComponent<TransformComponent>()->setPosition({ 0.0f, 9.0f, -23.0f });
    cameraObject->addComponent<FreeCameraComponent>();

    GameObject* object = new GameObject("ModelObject");
    object->addComponent<TransformComponent>();
    object->addComponent<FbxRenderComponent>("Data/Model/Jammo/Jammo.fbx");
    object->addComponent<AnimationComponent>();

    GameObject* stage = new GameObject("Stage");
    stage->addComponent<TransformComponent>();
    stage->addComponent<FbxRenderComponent>("Data/Model/Grass.fbx");

    GameObject* skyboxObj = new GameObject("Skybox");
    auto* skybox = skyboxObj->addComponent<SkyboxComponent>();
    skybox->setCubemap(L"Data/Texture/test.dds");

    GameObject* postEffectObj = new GameObject("PostEffectVolume");
    postEffectObj->addComponent<TransformComponent>();
    postEffectObj->addTag(Tag::PostEffect);
    auto* pe = postEffectObj->addComponent<PostEffectComponent>();
    pe->addEffect<BloomEffect>();
    pe->addEffect<ColorGradingEffect>();
    //pe->addEffect<DepthOfFieldEffect>();
    pe->addEffect<MotionBlurEffect>();

    GameObject* gpuEffectObj = new GameObject("GpuEffect");
    gpuEffectObj->addComponent<TransformComponent>();
    auto* gpuEffect = gpuEffectObj->addComponent<GpuEffectComponent>();
    gpuEffect->setTexture(L"Data/Texture/particle.png");
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