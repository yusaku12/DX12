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
    //! グリッドは毎フレーム描画リクエストを出す
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
    //! ファイル読み込みボタン
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

    //! FbxRender のデバッグウィンドウ
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

    //! wstring → string 変換（WideCharToMultiByte で安全に変換）
    const auto& wpath = paths[0];
    int size = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), static_cast<int>(wpath.size()), nullptr, 0, nullptr, nullptr);
    std::string path(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), static_cast<int>(wpath.size()), path.data(), size, nullptr, nullptr);

    LOG_INFO("[ModelEditor] Opening: %s", path.c_str());

    //! 新しい FbxRender を生成（前のものは自動解放）
    m_fbxRender = std::make_unique<FbxRender>(path);
    m_fbxRender->setTransform(m_transform);
    m_fbxRender->setShowBounds(true);
}