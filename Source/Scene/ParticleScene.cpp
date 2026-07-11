#include "pch.h"
#include "ParticleScene.h"
#include "Component\TransformComponent.h"
#include "Camera\FreeCameraComponent.h"
#include "Component\PostEffectComponent.h"
#include "PostEffect\BloomEffect.h"
#include "PostEffect\CasSharpenEffect.h"
#include "PostEffect\ColorGradingEffect.h"
#include "PostEffect\GTAOEffect.h"
#include "PostEffect\MotionBlurEffect.h"
#include "PostEffect\SSREffect.h"
#include "PostEffect\TemporalAAEffect.h"
#include "Component\GpuEffectComponent.h"
#include "Component\CpuParticleComponent.h"
#include "Component\SkyboxComponent.h"
#include "Component\CanvasComponent.h"
#include "Component\RectTransformComponent.h"
#include "Component\UIButtonComponent.h"
#include "Component\UITextComponent.h"
#include "Component\UIPanelComponent.h"
#include "Scene\UISetupTemplate.h"

void ParticleScene::onEnter()
{
    GameObject* cameraObject = DX_NEW(GameObject, "MainCamera");
    cameraObject->addComponent<TransformComponent>()->setPosition({ 0.0f, 9.0f, -23.0f });
    cameraObject->addComponent<FreeCameraComponent>();

    GameObject* postEffectObj = DX_NEW(GameObject, "PostEffectVolume");
    postEffectObj->addComponent<TransformComponent>();
    postEffectObj->addTag(Tag::PostEffect);
    auto* pe = postEffectObj->addComponent<PostEffectComponent>();
    pe->addEffect<GTAOEffect>();
    pe->addEffect<SSREffect>();
    pe->addEffect<BloomEffect>();
    pe->addEffect<MotionBlurEffect>();
    pe->addEffect<TemporalAAEffect>();
    pe->addEffect<ColorGradingEffect>();
    pe->addEffect<CasSharpenEffect>();

    GameObject* skyboxObj = DX_NEW(GameObject, "Skybox");
    auto* skybox = skyboxObj->addComponent<SkyboxComponent>();
    skybox->setCubemap(L"Data/Texture/test.dds");
    skyboxObj->setEnabled(false);

    GameObject* gpuEffectObj = DX_NEW(GameObject, "GpuEffect");
    gpuEffectObj->addComponent<TransformComponent>();
    auto* gpuEffect = gpuEffectObj->addComponent<GpuEffectComponent>();
    gpuEffect->setTexture(L"Data/Texture/particle.png");

    GameObject* cpuEffectObj = DX_NEW(GameObject, "CpuEffect");
    cpuEffectObj->addComponent<TransformComponent>()->setPosition({ 3.0f, 0.0f, 0.0f });
    auto* cpuEffect = cpuEffectObj->addComponent<CpuParticleComponent>();
    cpuEffect->setTexture(L"Data/Texture/particle.png");

    // ===== UI セットアップ =====
    // ★ステップ 1: Canvas を作成（UI 描画用コンテナ）
    GameObject* uiCanvas = UISetup::createScreenCanvas("UICanvas");

    // ★ステップ 2: UI パネルを追加（背景）
    UISetup::addPanelToCanvas(uiCanvas, "Panel_Background", 
        50.0f, 50.0f, 300.0f, 250.0f);

    // ★ステップ 3: タイトルテキストを追加
    UISetup::addTextToCanvas(uiCanvas, "Text_Title", 
        "Particle Scene", 60.0f, 70.0f, 280.0f, 50.0f);

    // ★ステップ 4: ボタンを追加
    GameObject* testButton = UISetup::addButtonToCanvas(uiCanvas, "Button_Test",
        80.0f, 150.0f, 240.0f, 50.0f);
    if (auto* btn = testButton->getComponent<UIButtonComponent>())
    {
        btn->setLabel("Test UI Button");
        btn->setClickEventName("ParticleScene.TestButton");
    }

    // ★ステップ 5: イベントハンドリング（オプション）
    EventBus::Instance().subscribe("ParticleScene.TestButton",
        [](const EventBus::Event&)
        {
            LOG_INFO("Particle Scene: Test button clicked!");
        });
}

void ParticleScene::update()
{
    // グリッドは毎フレーム描画リクエストを出す
    DebugPrimitive::Instance().drawGrid({ 0.0f,0.0f,0.0f }, 40.0f, 40.0f, 1.0f, { 0.5f,0.5f,0.5f,1.0f });
}

void ParticleScene::debugDraw()
{
}