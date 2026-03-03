#include "pch.h"
#include "TestScene.h"
#include "GameObject\GameObject.h"
#include "Component\TransformComponent.h"

void TestScene::onEnter()
{
    //! PMX
    {
        //! GameObject 作成して TransformComponent を追加（PMXモデルに紐付ける）
        GameObject* modelObj = new GameObject("PMX_Model_Object");
        TransformComponent* tf = modelObj->addComponent<TransformComponent>();
        tf->setPosition(Vector3(-7.0f, 0.0f, 0.0f));
        tf->setRotation(Quaternion::Identity);
        tf->setScale(Vector3::One);

        //! PMXモデルの描画（レンダラに Transform を設定）
        m_pmxRender = std::make_unique<PMXRender>(L"Data/Model/千夏/千夏皮肤.pmx");
        m_pmxRender->setTransform(tf);
    }

    //! FBX
    {
        //! GameObject 作成して TransformComponent を追加（PMXモデルに紐付ける）
        GameObject* modelObj = new GameObject("FBX_Model_Object");
        TransformComponent* tf = modelObj->addComponent<TransformComponent>();
        tf->setPosition(Vector3(7.0f, 0.0f, 0.0f));
        tf->setRotation(Quaternion::Identity);
        tf->setScale(Vector3::One);

        //! PMXモデルの描画（レンダラに Transform を設定）
        m_fbxRender = std::make_unique<FBXRender>("Data/Model/Jammo/Jammo.fbx");
        m_fbxRender->setTransform(tf);
    }

    //! デバック描画
    DebugPrimitive::Instance().addGrid({ 0,0,0 }, 100.0f, 100.0f, 1.0f, { 0.5f,0.5f,0.5f,1.0f }, 0.0f);
}

void TestScene::onExit()
{
}

void TestScene::update()
{
}

void TestScene::draw()
{
    //! PMXモデルの描画
    m_pmxRender->render();

    //! FBXモデルの描画
    m_fbxRender->render();
}

void TestScene::debugDraw()
{
    m_pmxRender->debugRender();
    m_fbxRender->debugRender();
}