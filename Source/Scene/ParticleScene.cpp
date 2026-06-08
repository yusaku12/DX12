#include "pch.h"
#include "ParticleScene.h"
#include "Component\TransformComponent.h"
#include "Camera\FreeCameraComponent.h"
#include "Component\PostEffectComponent.h"
#include "PostEffect\BloomEffect.h"
#include "PostEffect\ColorGradingEffect.h"
#include "Component\GpuEffectComponent.h"
#include "Component\SkyboxComponent.h"

void ParticleScene::onEnter()
{
    GameObject* cameraObject = new GameObject("MainCamera");
    cameraObject->addComponent<TransformComponent>()->setPosition({ 0.0f, 9.0f, -23.0f });
    cameraObject->addComponent<FreeCameraComponent>();

    GameObject* postEffectObj = new GameObject("PostEffectVolume");
    postEffectObj->addComponent<TransformComponent>();
    postEffectObj->addTag(Tag::PostEffect);
    auto* pe = postEffectObj->addComponent<PostEffectComponent>();
    pe->addEffect<BloomEffect>();
    pe->addEffect<ColorGradingEffect>();

    GameObject* skyboxObj = new GameObject("Skybox");
    auto* skybox = skyboxObj->addComponent<SkyboxComponent>();
    skybox->setCubemap(L"Data/Texture/test.dds");
    skyboxObj->setEnabled(false);

    GameObject* gpuEffectObj = new GameObject("GpuEffect");
    gpuEffectObj->addComponent<TransformComponent>();
    auto* gpuEffect = gpuEffectObj->addComponent<GpuEffectComponent>();
    gpuEffect->setTexture(L"Data/Texture/particle.png");
}

void ParticleScene::update()
{
    // グリッドは毎フレーム描画リクエストを出す
    DebugPrimitive::Instance().drawGrid({ 0.0f,0.0f,0.0f }, 40.0f, 40.0f, 1.0f, { 0.5f,0.5f,0.5f,1.0f });
}

void ParticleScene::debugDraw()
{
}