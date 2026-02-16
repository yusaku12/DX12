#include "pch.h"
#include "TestScene.h"

void TestScene::onEnter()
{
    //! テストポリゴン生成
    m_testPolygon = std::make_unique<TestPolygon>();
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
    m_testPolygon->render();
}