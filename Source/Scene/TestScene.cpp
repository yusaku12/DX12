#include "pch.h"
#include "TestScene.h"

void TestScene::onEnter()
{
    //! テストポリゴン生成
    //m_testPolygon = std::make_unique<TestPolygon>();

    //! PMXモデルの描画
    m_pmxRender = std::make_unique<PMXRender>(L"Data/Model/Kazusa_ByPOWER_v1.0/Kazusa_ByPOWER.pmx");
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