#include "pch.h"
#include "HierarchyWindow.h"
#include "EditorContext.h"
#include "GameObject\GameObject.h"

//! 再帰描画
static void drawGameObjectNode(GameObject* obj)
{
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanFullWidth;

    if (g_editor.selectedObject == obj)
        flags |= ImGuiTreeNodeFlags_Selected;

    bool opened = ImGui::TreeNodeEx(
        (void*)obj,
        flags,
        "%s",
        obj->getName().c_str()
    );

    //! 選択
    if (ImGui::IsItemClicked())
    {
        g_editor.selectedObject = obj;
    }

    //! Drag Source
    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload(
            "DND_GAMEOBJECT",
            &obj,
            sizeof(GameObject*)
        );
        ImGui::Text("%s", obj->getName().c_str());
        ImGui::EndDragDropSource();
    }

    //! Drop Target（親子）
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
        {
            GameObject* dropped = *(GameObject**)payload->Data;
            dropped->setParent(obj);
        }
        ImGui::EndDragDropTarget();
    }

    //! 右クリック Create
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Create Empty"))
        {
            auto* child = new GameObject("GameObject");
            child->setParent(obj);
        }
        ImGui::EndPopup();
    }

    //! 子を再帰描画
    if (opened)
    {
        for (GameObject* child : obj->getChildren())
        {
            drawGameObjectNode(child);
        }
        ImGui::TreePop();
    }
}

//! Hierarchy Window 本体
void drawHierarchyWindow()
{
    ImGui::Begin("Hierarchy");

    const auto& objects = GameObjectRegistry::Instance().getAll();

    //! 親を持たない GameObject をルート表示
    for (GameObject* obj : objects)
    {
        if (!obj->getParent())
        {
            drawGameObjectNode(obj);
        }
    }

    //! 空白右クリック（ルート作成）
    if (ImGui::BeginPopupContextWindow())
    {
        if (ImGui::MenuItem("Create Empty"))
        {
            new GameObject("GameObject");
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}