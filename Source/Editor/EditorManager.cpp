#include "pch.h"
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
    drawHierarchyWindow();
    drawInspectorWindow();
    drawCubemapToolWindow();
}