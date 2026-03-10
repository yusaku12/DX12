#include "pch.h"
#include "ModelEditorScene.h"

void ModelEditorScene::onEnter()
{
    GameObject* modelObj = new GameObject("FBX_Model_Object");
    m_transform = modelObj->addComponent<TransformComponent>();
}

void ModelEditorScene::onExit()
{
}

void ModelEditorScene::update()
{
    // グリッドは毎フレーム描画リクエストを出す
    DebugPrimitive::Instance().drawGrid({ 0.0f,0.0f,0.0f }, 100.0f, 100.0f, 1.0f, { 0.5f,0.5f,0.5f,1.0f });
}

void ModelEditorScene::draw()
{
}

void ModelEditorScene::drawMultiThreaded()
{
    if (m_fbxRender && m_fbxRender->isValid())
    {
        m_fbxRender->render();
    }
}

void ModelEditorScene::debugDraw()
{
    // ファイル読み込みボタン
    if (ImGui::Begin("Model Editor"))
    {
        if (ImGui::Button("Open FBX..."))
        {
            openFbxFile();
        }

        if (m_fbxRender)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", m_fbxRender->getRenderName());
        }
    }
    ImGui::End();

    // FbxRender のデバッグウィンドウ
    if (m_fbxRender)
    {
        m_fbxRender->debugImGui();
    }
}

void ModelEditorScene::openFbxFile()
{
    std::vector<std::wstring> paths;
    auto result = Dialog::openFile(paths, L"FBX ファイルを開く");

    if (result != DialogResult::OK || paths.empty())
        return;

    // 新しい FbxRender を生成（前のものは自動解放）
    m_fbxRender = std::make_unique<FbxRender>(wstringToString(paths[0]));
    m_fbxRender->setTransform(m_transform);
}