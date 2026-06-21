#include "pch.h"
#include "EditorContext.h"
#include "Component/FbxRenderComponent.h"
#include "GameObject\GameObject.h"
#include "Scene/SerializationCommon.h"

void drawInspectorWindow()
{
    ImGui::Begin("Inspector");

    if (g_editor.selectedObject)
    {
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