#pragma once

#include "GameObject\GameObject.h"
#include "Component\CanvasComponent.h"
#include "Component\RectTransformComponent.h"
#include "Component\UIButtonComponent.h"
#include "Component\UITextComponent.h"
#include "Component\UIPanelComponent.h"
#include "System\EventBus.h"

//=====================================================
//! UI セットアップ用テンプレート
//! Scene::onEnter() で使用する正しいパターン
//=====================================================

namespace UISetup
{
    //! 基本的なスクリーン UI Canvas を作成
    inline GameObject* createScreenCanvas(const std::string& name = "Canvas")
    {
        GameObject* canvasGO = new GameObject(name);
        auto* canvas = canvasGO->addComponent<CanvasComponent>();
        canvas->setRenderMode(CanvasRenderMode::ScreenOverlay);
        canvas->setSortOrder(0);
        canvas->setReceivesInput(true);
        
        // ★重要★ GameObject が enabled 状態になる時に onEnable() が呼ばれる
        // デフォルトで enabled なので Canvas は自動で RuntimeUIManager に登録される
        return canvasGO;
    }

    //! スクリーン Canvas にボタンを追加
    inline GameObject* addButtonToCanvas(
        GameObject* canvasParent,
        const std::string& buttonName,
        float x, float y, float w, float h)
    {
        if (!canvasParent) return nullptr;

        // ★重要★ Canvas を親として指定
        GameObject* buttonGO = new GameObject(buttonName);
        buttonGO->setParent(canvasParent);
        
        // ★必須★ UI コンポーネントには RectTransformComponent が必須
        auto* rectTransform = buttonGO->addComponent<RectTransformComponent>();
        rectTransform->setPosition(Vector2(x, y));
        rectTransform->setSize(Vector2(w, h));
        
        // ボタンコンポーネント
        auto* button = buttonGO->addComponent<UIButtonComponent>();
        button->setLabel("Button");
        button->setNormalColor(Vector4(0.2f, 0.2f, 0.2f, 0.9f));
        button->setHoverColor(Vector4(0.3f, 0.3f, 0.3f, 0.95f));
        button->setPressedColor(Vector4(0.1f, 0.1f, 0.1f, 1.0f));
        
        return buttonGO;
    }

    //! スクリーン Canvas にテキストを追加
    inline GameObject* addTextToCanvas(
        GameObject* canvasParent,
        const std::string& textName,
        const std::string& content,
        float x, float y, float w, float h)
    {
        if (!canvasParent) return nullptr;

        // ★重要★ Canvas を親として指定
        GameObject* textGO = new GameObject(textName);
        textGO->setParent(canvasParent);
        
        // ★必須★ UI コンポーネントには RectTransformComponent が必須
        auto* rectTransform = textGO->addComponent<RectTransformComponent>();
        rectTransform->setPosition(Vector2(x, y));
        rectTransform->setSize(Vector2(w, h));
        
        // テキストコンポーネント
        auto* text = textGO->addComponent<UITextComponent>();
        text->setText(content);
        text->setColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        text->setAlignment(UITextAlignment::MiddleCenter);
        text->setFontScale(1.0f);
        
        return textGO;
    }

    //! スクリーン Canvas にパネルを追加
    inline GameObject* addPanelToCanvas(
        GameObject* canvasParent,
        const std::string& panelName,
        float x, float y, float w, float h)
    {
        if (!canvasParent) return nullptr;

        // ★重要★ Canvas を親として指定
        GameObject* panelGO = new GameObject(panelName);
        panelGO->setParent(canvasParent);
        
        // ★必須★ UI コンポーネントには RectTransformComponent が必須
        auto* rectTransform = panelGO->addComponent<RectTransformComponent>();
        rectTransform->setPosition(Vector2(x, y));
        rectTransform->setSize(Vector2(w, h));
        
        // パネルコンポーネント
        auto* panel = panelGO->addComponent<UIPanelComponent>();
        panel->setBackgroundColor(Vector4(0.1f, 0.1f, 0.1f, 0.8f));
        panel->setBorderColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        panel->setBorderWidth(2.0f);
        
        return panelGO;
    }
}

// =============================================================
//  使用例
// =============================================================
/*
void MyScene::onEnter()
{
    // ★ステップ 1: Canvas を作成
    GameObject* canvas = UISetup::createScreenCanvas("MyUICanvas");

    // ★ステップ 2: UI コンポーネントを追加
    GameObject* button = UISetup::addButtonToCanvas(canvas, "StartButton", 100, 100, 200, 50);
    GameObject* text = UISetup::addTextToCanvas(canvas, "Title", "Welcome!", 100, 200, 400, 60);
    GameObject* panel = UISetup::addPanelToCanvas(canvas, "Background", 50, 50, 500, 400);

    // ★ステップ 3: イベントハンドリング（オプション）
    EventBus::Instance().subscribe("UI.Button.Click",
        [](const EventBus::Event& e)
        {
            if (const auto* payload = e.payloadAs<UIButtonClickEvent>())
            {
                LOG_INFO("Button clicked: %s", payload->buttonObjectName.c_str());
            }
        });
}

// ★重要★ 描画は Window.cpp で自動的に行われます:
// 1. Window.cpp::render() で SceneManager::draw() が呼ばれる
// 2. その後 RuntimeUIManager::renderNative() が呼ばれる
// 3. RuntimeUIManager が Canvas.enabled に基づいて自動的に描画
*/
