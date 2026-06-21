#include "pch.h"
#include "HierarchyWindow.h"
#include "AssetDragDrop.h"
#include "EditorContext.h"
#include "GameObject\GameObject.h"
#include "Scene\PrefabFlatBuffer.h"

namespace
{
    bool isDescendantOf(GameObject* node, GameObject* ancestor)
    {
        GameObject* current = node;
        while (current)
        {
            if (current == ancestor)
            {
                return true;
            }

            current = current->getParent();
        }

        return false;
    }
}

static void drawGameObjectNode(GameObject* obj)
{
    if (!obj || obj->isDestroyed())
        return;

    // 子の有無を確認
    const auto& children = obj->getChildren();
    bool hasChildren = !children.empty();

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanFullWidth;

    // 子がいなければ矢印を表示しない（リーフ）
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf;

    if (g_editor.selectedObject == obj)
        flags |= ImGuiTreeNodeFlags_Selected;

    bool opened = ImGui::TreeNodeEx(
        (void*)obj,
        flags,
        "%s",
        obj->getName().c_str()
    );

    // Item 右クリックメニュー
    if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
    {
        if (ImGui::MenuItem("Save As Prefab..."))
        {
            std::wstring outPath;
            if (Dialog::saveFile(outPath, L"Save Prefab", L"", L"prefab") == DialogResult::OK && !outPath.empty())
            {
                PrefabFlatBuffer::save(std::filesystem::path(outPath), obj);
            }
        }

        if (ImGui::MenuItem("Instantiate Prefab As Child..."))
        {
            std::vector<std::wstring> paths;
            if (Dialog::openFile(paths, L"Load Prefab", L"", false) == DialogResult::OK && !paths.empty())
            {
                if (GameObject* instance = PrefabFlatBuffer::instantiate(std::filesystem::path(paths.front()), obj))
                {
                    g_editor.selectedObject = instance;
                }
            }
        }

        GameObject* prefabRoot = PrefabFlatBuffer::findPrefabRoot(obj);
        if (prefabRoot)
        {
            if (ImGui::MenuItem("Apply Prefab"))
            {
                PrefabFlatBuffer::apply(obj);
            }

            if (ImGui::MenuItem("Revert Prefab"))
            {
                const bool selectedInPrefab = isDescendantOf(g_editor.selectedObject, prefabRoot);
                if (GameObject* replaced = PrefabFlatBuffer::revert(obj))
                {
                    if (selectedInPrefab)
                    {
                        g_editor.selectedObject = replaced;
                    }
                }
                else if (selectedInPrefab)
                {
                    g_editor.selectedObject = nullptr;
                }
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Delete"))
        {
            if (g_editor.selectedObject == obj)
                g_editor.selectedObject = nullptr;

            obj->destroy();
        }

        ImGui::EndPopup();
    }

    // 選択
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        g_editor.selectedObject = obj;
    }

    // Drag Source
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

    // Drop Target（親子付け替え）
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
        {
            GameObject* dropped = *(GameObject**)payload->Data;

            // 自分自身・破棄予定は弾く
            if (dropped && dropped != obj && !dropped->isDestroyed())
            {
                dropped->setParent(obj);
            }
        }

        EditorAssetDragDrop::acceptAssetDropInCurrentTarget(obj);
        ImGui::EndDragDropTarget();
    }

    // 子ノード再帰描画
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

    // ルート GameObject 表示
    for (GameObject* obj : objects)
    {
        if (obj && !obj->getParent() && !obj->isDestroyed())
        {
            drawGameObjectNode(obj);
        }
    }

    // ルートへのドロップ領域（ドラッグ中のみ表示して通常 UI を邪魔しない）
    if (ImGui::GetDragDropPayload() != nullptr)
    {
        const ImVec2 rootDropSize = ImVec2(-FLT_MIN, std::max(28.0f, ImGui::GetContentRegionAvail().y));
        ImGui::InvisibleButton("##HierarchyRootDrop", rootDropSize);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Drop Here To Add As Root");
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
            {
                GameObject* dropped = *(GameObject**)payload->Data;
                if (dropped && !dropped->isDestroyed())
                {
                    dropped->setParent(nullptr);
                }
            }

            EditorAssetDragDrop::acceptAssetDropInCurrentTarget(nullptr);
            ImGui::EndDragDropTarget();
        }
    }

    // 空白右クリック（ルート作成）
    if (ImGui::BeginPopupContextWindow(
        nullptr,
        ImGuiPopupFlags_NoOpenOverItems |
        ImGuiPopupFlags_MouseButtonRight))
    {
        if (ImGui::MenuItem("Create Empty"))
        {
            new GameObject("GameObject");
        }

        if (ImGui::MenuItem("Instantiate Prefab..."))
        {
            std::vector<std::wstring> paths;
            if (Dialog::openFile(paths, L"Load Prefab", L"", false) == DialogResult::OK && !paths.empty())
            {
                if (GameObject* instance = PrefabFlatBuffer::instantiate(std::filesystem::path(paths.front()), nullptr))
                {
                    g_editor.selectedObject = instance;
                }
            }
        }

        ImGui::EndPopup();
    }
    ImGui::End();
}