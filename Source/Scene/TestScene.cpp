#include "pch.h"
#include "TestScene.h"

void TestScene::onEnter()
{
    //! テストポリゴン生成
    //m_testPolygon = std::make_unique<TestPolygon>();

    //! PMXモデルの描画
    m_pmxRender = std::make_unique<PMXRender>(L"Data/Model/千夏/千夏皮肤.pmx");
}

void TestScene::onExit()
{
}

void TestScene::update()
{
}

void TestScene::draw()
{
    //! テストポリゴン描画
    //m_testPolygon->render();

    //! PMXモデルの描画
    m_pmxRender->render();
}

void TestScene::debugDraw()
{
    m_pmxRender->debugRender();
}