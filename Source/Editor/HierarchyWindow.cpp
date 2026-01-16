#include "pch.h"
#include "HierarchyWindow.h"
#include "EditorContext.h"
#include "GameObject\GameObject.h"

//! GameObject ノード再帰描画
static void drawGameObjectNode(GameObject* obj)
{
    if (!obj || obj->isDestroyed())
        return;

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

    //! Item 右クリックメニュー
    if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
    {
        if (ImGui::MenuItem("Delete"))
        {
            if (g_editor.selectedObject == obj)
                g_editor.selectedObject = nullptr;

            obj->destroy();
        }

        ImGui::EndPopup();
    }

    //! 選択
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
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

    //! Drop Target（親子付け替え）
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
        {
            GameObject* dropped = *(GameObject**)payload->Data;

            //! 自分自身・破棄予定は弾く
            if (dropped && dropped != obj && !dropped->isDestroyed())
            {
                dropped->setParent(obj);
            }
        }
        ImGui::EndDragDropTarget();
    }

    //! 子ノード再帰描画
    if (opened)
    {
        for (GameObject* child : obj->getChildren())
        {
            drawGameObjectNode(child);
        }
        ImGui::TreePop();
    }
}

void drawHierarchyWindow()
{
    ImGui::Begin("Hierarchy");

    const auto& objects = GameObjectRegistry::Instance().getAll();

    //! ルート GameObject 表示
    for (GameObject* obj : objects)
    {
        if (obj && !obj->getParent() && !obj->isDestroyed())
        {
            drawGameObjectNode(obj);
        }
    }

    //! 空白右クリック（ルート作成）
    if (g_editor.selectedObject == nullptr &&
        ImGui::BeginPopupContextWindow(
            nullptr,
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight))
    {
        if (ImGui::MenuItem("Create Empty"))
        {
            new GameObject("GameObject");
        }

        ImGui::EndPopup();
    }
    ImGui::End();
}