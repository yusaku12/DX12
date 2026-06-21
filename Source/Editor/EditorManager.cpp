#include "pch.h"
#include "AssetBrowserWindow.h"
#include "HierarchyWindow.h"
#include "InspectorWindow.h"
#include "CubemapToolWindow.h"
#include "GameObject\ObjectPicker.h"

void EditorManager::update()
{
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