#include "pch.h"
#include "EditorContext.h"
#include "EditorTransaction.h"
#include "Component/FbxRenderComponent.h"
#include "Component/UIImageComponent.h"
#include "Component/RectTransformComponent.h"
#include "GameObject\GameObject.h"
#include "Scene/PrefabFlatBuffer.h"
#include "Scene/SerializationCommon.h"

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

void drawInspectorWindow()
{
    ImGui::Begin("Inspector");

    auto& transactions = EditorTransaction::Manager::Instance();
    const bool canUndo = transactions.canUndo();
    const bool canRedo = transactions.canRedo();

    if (!canUndo) ImGui::BeginDisabled();
    if (ImGui::Button("Undo"))
    {
        transactions.undo();
    }
    if (!canUndo) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!canRedo) ImGui::BeginDisabled();
    if (ImGui::Button("Redo"))
    {
        transactions.redo();
    }
    if (!canRedo) ImGui::EndDisabled();

    if (canUndo || canRedo)
    {
        ImGui::TextDisabled("Undo: %s | Redo: %s",
            canUndo ? transactions.nextUndoLabel() : "(none)",
            canRedo ? transactions.nextRedoLabel() : "(none)");
    }

    ImGui::Separator();

    if (g_editor.selectedObject)
    {
        GameObject* prefabRoot = PrefabFlatBuffer::findPrefabRoot(g_editor.selectedObject);
        if (prefabRoot)
        {
            ImGui::SeparatorText("Prefab");
            ImGui::Text("Instance Root: %s", prefabRoot->getName().c_str());
            ImGui::TextWrapped("Asset: %s", prefabRoot->getPrefabAssetPath().c_str());

            if (ImGui::Button("Apply Prefab"))
            {
                PrefabFlatBuffer::apply(g_editor.selectedObject);
            }

            ImGui::SameLine();
            if (ImGui::Button("Revert Prefab"))
            {
                const bool selectedInPrefab = isDescendantOf(g_editor.selectedObject, prefabRoot);
                if (GameObject* replaced = PrefabFlatBuffer::revert(g_editor.selectedObject))
                {
                    if (selectedInPrefab)
                    {
                        g_editor.selectedObject = replaced;
                    }
                }
            }

            if (ImGui::Button("Create Variant..."))
            {
                std::wstring outPath;
                if (Dialog::saveFile(outPath, L"Create Prefab Variant", L"", L"prefab") == DialogResult::OK && !outPath.empty())
                {
                    PrefabFlatBuffer::createVariant(
                        std::filesystem::path(outPath),
                        std::filesystem::path(prefabRoot->getPrefabAssetPath()),
                        prefabRoot);
                }
            }

            PrefabFlatBuffer::OverrideInfo overrideInfo;
            if (PrefabFlatBuffer::buildOverrideInfo(g_editor.selectedObject, overrideInfo) && overrideInfo.valid)
            {
                if (overrideInfo.isVariant)
                {
                    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "Variant Base: %s", overrideInfo.basePrefabPath.string().c_str());
                }

                ImGui::Text("Compare Target: %s", overrideInfo.compareTargetPath.string().c_str());

                if (overrideInfo.entries.empty())
                {
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Overrides: None");
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.2f, 1.0f), "Overrides: %d", static_cast<int>(overrideInfo.entries.size()));
                    if (ImGui::TreeNode("Override Details"))
                    {
                        for (const auto& entry : overrideInfo.entries)
                        {
                            ImGui::BulletText("%s - %s", entry.objectName.c_str(), entry.detail.c_str());
                        }
                        ImGui::TreePop();
                    }
                }
            }

            ImGui::Separator();
        }

        if (ImGui::Button("Add Component"))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            static char filter[64] = "";
            ImGui::InputTextWithHint("##ComponentFilter", "Search component...", filter, IM_ARRAYSIZE(filter));
            ImGui::Separator();

            if ((filter[0] == '\0' || std::string("FbxRenderComponent").find(filter) != std::string::npos)
                && !SerializationCommon::hasComponentByTypeName(g_editor.selectedObject, "FbxRenderComponent"))
            {
                if (ImGui::MenuItem("FbxRenderComponent..."))
                {
                    std::vector<std::wstring> paths;
                    if (Dialog::openFile(paths, L"Select FBX", L"", false) == DialogResult::OK && !paths.empty())
                    {
                        const std::string modelPath = wstringToString(paths.front());
                        Component* added = g_editor.selectedObject->addComponent<FbxRenderComponent>(modelPath);
                        if (!added)
                        {
                            LOG_WARN("[Inspector] Failed to add FbxRenderComponent");
                        }
                    }
                }
            }

            if ((filter[0] == '\0' || std::string("UIImageComponent").find(filter) != std::string::npos)
                && !SerializationCommon::hasComponentByTypeName(g_editor.selectedObject, "UIImageComponent"))
            {
                if (ImGui::MenuItem("UIImageComponent..."))
                {
                    std::vector<std::wstring> paths;
                    if (Dialog::openFile(paths, L"Select Texture", L"", false) == DialogResult::OK && !paths.empty())
                    {
                        // RectTransformComponent がなければ追加
                        if (!g_editor.selectedObject->getComponent<RectTransformComponent>())
                        {
                            RectTransformComponent* rt = g_editor.selectedObject->addComponent<RectTransformComponent>();
                            if (rt)
                            {
                                rt->setSize(Vector2(100.f, 100.f));  // デフォルトサイズ
                            }
                        }

                        UIImageComponent* added = g_editor.selectedObject->addComponent<UIImageComponent>();
                        if (added)
                        {
                            added->setTexturePath(paths.front());
                        }
                        else
                        {
                            LOG_WARN("[Inspector] Failed to add UIImageComponent");
                        }
                    }
                }
            }

            for (const auto& archetype : SerializationCommon::getComponentArchetypes())
            {
                if (!archetype.addableInEditor)
                {
                    continue;
                }

                const std::string label = archetype.typeName;
                if (filter[0] != '\0' && label.find(filter) == std::string::npos)
                {
                    continue;
                }

                const bool alreadyExists = SerializationCommon::hasComponentByTypeName(g_editor.selectedObject, archetype.typeName);
                if (alreadyExists)
                {
                    ImGui::BeginDisabled();
                    ImGui::MenuItem((label + " (Added)").c_str(), nullptr, false, false);
                    ImGui::EndDisabled();
                    continue;
                }

                if (ImGui::MenuItem(label.c_str()))
                {
                    Component* added = SerializationCommon::addComponentByTypeName(g_editor.selectedObject, archetype.typeName);
                    if (!added)
                    {
                        LOG_WARN("[Inspector] Failed to add component: %s", archetype.typeName);
                    }
                }
            }

            ImGui::EndPopup();
        }

        ImGui::Separator();

        // Component 一覧を描画
        g_editor.selectedObject->drawInspector();
    }
    else
    {
        ImGui::TextDisabled("No GameObject Selected");
    }

    ImGui::End();
}