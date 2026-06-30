#pragma once

#include "GameObject\GameObject.h"
#include "Component\CanvasComponent.h"
#include "Component\RectTransformComponent.h"
#include "Component\UIButtonComponent.h"
#include "Component\UITextComponent.h"
#include "Component\UIPanelComponent.h"
#include "Component\UIImageComponent.h"
#include "System\EventBus.h"

//=====================================================
//! UI 繧ｻ繝・ヨ繧｢繝・・逕ｨ繝・Φ繝励Ξ繝ｼ繝・
//! Scene::onEnter() 縺ｧ菴ｿ逕ｨ縺吶ｋ豁｣縺励＞繝代ち繝ｼ繝ｳ
//=====================================================

namespace UISetup
{
    //! 蝓ｺ譛ｬ逧・↑繧ｹ繧ｯ繝ｪ繝ｼ繝ｳ UI Canvas 繧剃ｽ懈・
    inline GameObject* createScreenCanvas(const std::string& name = "Canvas")
    {
        GameObject* canvasGO = DX_NEW(GameObject, name);
        auto* canvas = canvasGO->addComponent<CanvasComponent>();
        canvas->setRenderMode(CanvasRenderMode::ScreenOverlay);
        canvas->setSortOrder(0);
        canvas->setReceivesInput(true);
        
        // 笘・㍾隕≫・ GameObject 縺・enabled 迥ｶ諷九〒 addComponent 縺輔ｌ縺溘→縺阪・
        // onEnable() 縺瑚・蜍輔〒蜻ｼ縺ｰ繧後ｋ・・ameObject.h 縺ｧ螳溯｣・ｸ医∩・・
        // 縺薙ｌ縺ｫ繧医ｊ CanvasComponent::onEnable() 縺悟ｮ溯｡後＆繧後ヽuntimeUIManager 縺ｫ逋ｻ骭ｲ縺輔ｌ繧・
        
        return canvasGO;
    }

    //! 繧ｹ繧ｯ繝ｪ繝ｼ繝ｳ Canvas 縺ｫ繝懊ち繝ｳ繧定ｿｽ蜉
    inline GameObject* addButtonToCanvas(
        GameObject* canvasParent,
        const std::string& buttonName,
        float x, float y, float w, float h)
    {
        if (!canvasParent) return nullptr;

        // 笘・㍾隕≫・ Canvas 繧定ｦｪ縺ｨ縺励※謖・ｮ・
        GameObject* buttonGO = DX_NEW(GameObject, buttonName);
        buttonGO->setParent(canvasParent);
        
        // 笘・ｿ・遺・ UI 繧ｳ繝ｳ繝昴・繝阪Φ繝医↓縺ｯ RectTransformComponent 縺悟ｿ・・
        auto* rectTransform = buttonGO->addComponent<RectTransformComponent>();
        rectTransform->setPosition(Vector2(x, y));
        rectTransform->setSize(Vector2(w, h));
        
        // 繝懊ち繝ｳ繧ｳ繝ｳ繝昴・繝阪Φ繝・
        auto* button = buttonGO->addComponent<UIButtonComponent>();
        button->setLabel("Button");
        button->setNormalColor(Vector4(0.2f, 0.2f, 0.2f, 0.9f));
        button->setHoverColor(Vector4(0.3f, 0.3f, 0.3f, 0.95f));
        button->setPressedColor(Vector4(0.1f, 0.1f, 0.1f, 1.0f));
        
        return buttonGO;
    }

    //! 繧ｹ繧ｯ繝ｪ繝ｼ繝ｳ Canvas 縺ｫ繝・く繧ｹ繝医ｒ霑ｽ蜉
    inline GameObject* addTextToCanvas(
        GameObject* canvasParent,
        const std::string& textName,
        const std::string& content,
        float x, float y, float w, float h)
    {
        if (!canvasParent) return nullptr;

        // 笘・㍾隕≫・ Canvas 繧定ｦｪ縺ｨ縺励※謖・ｮ・
        GameObject* textGO = DX_NEW(GameObject, textName);
        textGO->setParent(canvasParent);
        
        // 笘・ｿ・遺・ UI 繧ｳ繝ｳ繝昴・繝阪Φ繝医↓縺ｯ RectTransformComponent 縺悟ｿ・・
        auto* rectTransform = textGO->addComponent<RectTransformComponent>();
        rectTransform->setPosition(Vector2(x, y));
        rectTransform->setSize(Vector2(w, h));
        
        // 繝・く繧ｹ繝医さ繝ｳ繝昴・繝阪Φ繝・
        auto* text = textGO->addComponent<UITextComponent>();
        text->setText(content);
        text->setColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        text->setAlignment(UITextAlignment::MiddleCenter);
        text->setFontScale(1.0f);
        
        return textGO;
    }

    //! 繧ｹ繧ｯ繝ｪ繝ｼ繝ｳ Canvas 縺ｫ繝代ロ繝ｫ繧定ｿｽ蜉
    inline GameObject* addPanelToCanvas(
        GameObject* canvasParent,
        const std::string& panelName,
        float x, float y, float w, float h)
    {
        if (!canvasParent) return nullptr;

        // 笘・㍾隕≫・ Canvas 繧定ｦｪ縺ｨ縺励※謖・ｮ・
        GameObject* panelGO = DX_NEW(GameObject, panelName);
        panelGO->setParent(canvasParent);
        
        // 笘・ｿ・遺・ UI 繧ｳ繝ｳ繝昴・繝阪Φ繝医↓縺ｯ RectTransformComponent 縺悟ｿ・・
        auto* rectTransform = panelGO->addComponent<RectTransformComponent>();
        rectTransform->setPosition(Vector2(x, y));
        rectTransform->setSize(Vector2(w, h));
        
        // 繝代ロ繝ｫ繧ｳ繝ｳ繝昴・繝阪Φ繝・
        auto* panel = panelGO->addComponent<UIPanelComponent>();
        panel->setBackgroundColor(Vector4(0.1f, 0.1f, 0.1f, 0.8f));
        panel->setBorderColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        panel->setBorderWidth(2.0f);
        
        return panelGO;
    }

    //! 繧ｹ繧ｯ繝ｪ繝ｼ繝ｳ Canvas 縺ｫ逕ｻ蜒上ｒ霑ｽ蜉
    inline GameObject* addImageToCanvas(
        GameObject* canvasParent,
        const std::string& imageName,
        const std::wstring& texturePath,
        float x, float y, float w, float h)
    {
        if (!canvasParent) return nullptr;

        // 笘・㍾隕≫・ Canvas 繧定ｦｪ縺ｨ縺励※謖・ｮ・
        GameObject* imageGO = DX_NEW(GameObject, imageName);
        imageGO->setParent(canvasParent);
        
        // 笘・ｿ・遺・ UI 繧ｳ繝ｳ繝昴・繝阪Φ繝医↓縺ｯ RectTransformComponent 縺悟ｿ・・
        auto* rectTransform = imageGO->addComponent<RectTransformComponent>();
        rectTransform->setPosition(Vector2(x, y));
        rectTransform->setSize(Vector2(w, h));
        
        // 逕ｻ蜒上さ繝ｳ繝昴・繝阪Φ繝・
        auto* image = imageGO->addComponent<UIImageComponent>();
        image->setTexturePath(texturePath);
        image->setTintColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        image->setAlpha(1.0f);
        
        return imageGO;
    }
}

// =============================================================
//  菴ｿ逕ｨ萓・
// =============================================================
/*
void MyScene::onEnter()
{
    // 笘・せ繝・ャ繝・1: Canvas 繧剃ｽ懈・
    GameObject* canvas = UISetup::createScreenCanvas("MyUICanvas");

    // 笘・せ繝・ャ繝・2: UI 繧ｳ繝ｳ繝昴・繝阪Φ繝医ｒ霑ｽ蜉
    GameObject* panel = UISetup::addPanelToCanvas(canvas, "Background", 50, 50, 500, 400);
    GameObject* image = UISetup::addImageToCanvas(canvas, "Logo", L"Data/Texture/logo.png", 100, 100, 200, 100);
    GameObject* text = UISetup::addTextToCanvas(canvas, "Title", "Welcome!", 100, 220, 400, 60);
    GameObject* button = UISetup::addButtonToCanvas(canvas, "StartButton", 150, 300, 200, 50);

    // 笘・せ繝・ャ繝・3: 繧､繝吶Φ繝医ワ繝ｳ繝峨Μ繝ｳ繧ｰ・医が繝励す繝ｧ繝ｳ・・
    EventBus::Instance().subscribe("UI.Button.Click",
        [](const EventBus::Event& e)
        {
            if (const auto* payload = e.payloadAs<UIButtonClickEvent>())
            {
                LOG_INFO("Button clicked: %s", payload->buttonObjectName.c_str());
            }
        });
}

// 笘・㍾隕≫・ 謠冗判縺ｯ Window.cpp 縺ｧ閾ｪ蜍慕噪縺ｫ陦後ｏ繧後∪縺・
// 1. Window.cpp::render() 縺ｧ SceneManager::draw() 縺悟他縺ｰ繧後ｋ
// 2. 縺昴・蠕・RuntimeUIManager::renderNative() 縺悟他縺ｰ繧後ｋ
// 3. RuntimeUIManager 縺・Canvas.enabled 縺ｫ蝓ｺ縺･縺・※閾ｪ蜍慕噪縺ｫ謠冗判
*/
