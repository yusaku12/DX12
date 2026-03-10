#include "pch.h"
#include "EditorContext.h"
#include "GameObject\GameObject.h"

void drawInspectorWindow()
{
    ImGui::Begin("Inspector");

    if (g_editor.selectedObject)
    {
        // Component 一覧を描画
        g_editor.selectedObject->drawInspector();
    }
    else
    {
        ImGui::TextDisabled("No GameObject Selected");
    }

    ImGui::End();
}