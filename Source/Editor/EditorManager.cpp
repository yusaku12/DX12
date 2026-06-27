#include "pch.h"
#include "AssetBrowserWindow.h"
#include "AsyncAssetLoader.h"
#include "EditorTransaction.h"
#include "HierarchyWindow.h"
#include "InspectorWindow.h"
#include "CubemapToolWindow.h"
#include "GameObject\ObjectPicker.h"

void EditorManager::update()
{
    EditorAsyncAsset::AsyncAssetLoader::Instance().update();

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
    {
        const bool ctrl = io.KeyCtrl;
        const bool shift = io.KeyShift;

        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            EditorTransaction::Manager::Instance().undo();
        }
        else if ((ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
            || (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)))
        {
            EditorTransaction::Manager::Instance().redo();
        }
    }

    // Scene ウィンドウ上でのオブジェクトピッキング
    ObjectPicker::Instance().update();
}

void EditorManager::imgui()
{
    drawAssetBrowserWindow();
    drawHierarchyWindow();
    drawInspectorWindow();
    //drawCubemapToolWindow();
}