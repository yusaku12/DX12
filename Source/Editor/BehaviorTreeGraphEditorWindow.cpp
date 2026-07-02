#include "pch.h"
#include "BehaviorTreeGraphEditorWindow.h"

#include "System/BehaviorTreeRuntime.h"

namespace
{
    ImVec2 addV2(const ImVec2& a, const ImVec2& b)
    {
        return ImVec2(a.x + b.x, a.y + b.y);
    }

    ImVec2 subV2(const ImVec2& a, const ImVec2& b)
    {
        return ImVec2(a.x - b.x, a.y - b.y);
    }

    struct GraphNode
    {
        int id = -1;
        BehaviorTreeRuntime::NodeType type = BehaviorTreeRuntime::NodeType::Sequence;
        ImVec2 position = ImVec2(40.0f, 40.0f);
        std::vector<int> children;
    };

    struct GraphState
    {
        char filePath[260] = "Data/AI/Default.btree";
        std::vector<GraphNode> nodes;
        int rootId = -1;
        int selectedId = -1;
        int nextNodeId = 1;
        int pendingChildTarget = -1;
        std::string status;
    };

    GraphState s_state{};

    const char* nodeTypeName(BehaviorTreeRuntime::NodeType type)
    {
        switch (type)
        {
        case BehaviorTreeRuntime::NodeType::Selector: return "Selector";
        case BehaviorTreeRuntime::NodeType::Sequence: return "Sequence";
        case BehaviorTreeRuntime::NodeType::ConditionHasDestination: return "ConditionHasDestination";
        case BehaviorTreeRuntime::NodeType::ConditionHasPath: return "ConditionHasPath";
        case BehaviorTreeRuntime::NodeType::ActionMoveToDestination: return "ActionMoveToDestination";
        case BehaviorTreeRuntime::NodeType::ActionStop: return "ActionStop";
        default: return "Unknown";
        }
    }

    GraphNode* findNodeById(int id)
    {
        for (GraphNode& node : s_state.nodes)
        {
            if (node.id == id)
            {
                return &node;
            }
        }

        return nullptr;
    }

    const GraphNode* findNodeByIdConst(int id)
    {
        for (const GraphNode& node : s_state.nodes)
        {
            if (node.id == id)
            {
                return &node;
            }
        }

        return nullptr;
    }

    void removeNode(int id)
    {
        s_state.nodes.erase(
            std::remove_if(s_state.nodes.begin(), s_state.nodes.end(), [id](const GraphNode& node) { return node.id == id; }),
            s_state.nodes.end());

        for (GraphNode& node : s_state.nodes)
        {
            node.children.erase(
                std::remove(node.children.begin(), node.children.end(), id),
                node.children.end());
        }

        if (s_state.rootId == id)
        {
            s_state.rootId = -1;
        }

        if (s_state.selectedId == id)
        {
            s_state.selectedId = -1;
        }
    }

    bool saveGraphToFile(const std::filesystem::path& filePath)
    {
        if (s_state.rootId < 0 || s_state.nodes.empty())
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);

        std::ofstream out(filePath, std::ios::trunc);
        if (!out)
        {
            return false;
        }

        out << "root " << s_state.rootId << "\n";

        for (const GraphNode& node : s_state.nodes)
        {
            out << "node " << node.id << " " << nodeTypeName(node.type) << "\n";
        }

        for (const GraphNode& node : s_state.nodes)
        {
            for (int child : node.children)
            {
                out << "child " << node.id << " " << child << "\n";
            }
        }

        return out.good();
    }

    bool loadGraphFromFile(const std::filesystem::path& filePath)
    {
        BehaviorTreeRuntime::Asset asset;
        if (!BehaviorTreeRuntime::loadAsset(filePath, asset))
        {
            return false;
        }

        s_state.nodes.clear();
        s_state.rootId = asset.rootNodeId;
        s_state.selectedId = -1;
        s_state.nextNodeId = 1;

        for (const auto& node : asset.nodes)
        {
            GraphNode graphNode;
            graphNode.id = node.id;
            graphNode.type = node.type;
            graphNode.children = node.children;
            graphNode.position = ImVec2(40.0f + static_cast<float>(s_state.nodes.size() % 4) * 180.0f,
                40.0f + static_cast<float>(s_state.nodes.size() / 4) * 120.0f);
            s_state.nodes.push_back(std::move(graphNode));
            s_state.nextNodeId = std::max(s_state.nextNodeId, node.id + 1);
        }

        return true;
    }

    void drawGraphCanvas()
    {
        ImGui::BeginChild("BTGraphCanvas", ImVec2(0.0f, 280.0f), true);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();

        for (const GraphNode& node : s_state.nodes)
        {
            const ImVec2 p0 = addV2(canvasPos, node.position);
            const ImVec2 p1 = addV2(p0, ImVec2(150.0f, 56.0f));

            const bool selected = (node.id == s_state.selectedId);
            const ImU32 fillColor = selected ? IM_COL32(80, 110, 180, 220) : IM_COL32(60, 60, 70, 220);
            drawList->AddRectFilled(p0, p1, fillColor, 6.0f);
            drawList->AddRect(p0, p1, IM_COL32(220, 220, 230, 180), 6.0f, 0, 1.5f);
            drawList->AddText(addV2(p0, ImVec2(8.0f, 8.0f)), IM_COL32(240, 240, 240, 255), std::format("#{}", node.id).c_str());
            drawList->AddText(addV2(p0, ImVec2(8.0f, 28.0f)), IM_COL32(220, 220, 180, 255), nodeTypeName(node.type));

            ImGui::SetCursorScreenPos(p0);
            ImGui::InvisibleButton(std::format("##Node{}", node.id).c_str(), ImVec2(150.0f, 56.0f));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                s_state.selectedId = node.id;
            }

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                GraphNode* mutableNode = findNodeById(node.id);
                if (mutableNode)
                {
                    mutableNode->position.x += ImGui::GetIO().MouseDelta.x;
                    mutableNode->position.y += ImGui::GetIO().MouseDelta.y;
                }
            }
        }

        for (const GraphNode& node : s_state.nodes)
        {
            const ImVec2 from = addV2(addV2(canvasPos, node.position), ImVec2(150.0f, 28.0f));
            for (int childId : node.children)
            {
                const GraphNode* child = findNodeByIdConst(childId);
                if (!child)
                {
                    continue;
                }

                const ImVec2 to = addV2(addV2(canvasPos, child->position), ImVec2(0.0f, 28.0f));
                drawList->AddBezierCubic(from, addV2(from, ImVec2(60.0f, 0.0f)), subV2(to, ImVec2(60.0f, 0.0f)), to, IM_COL32(200, 210, 220, 180), 2.0f);
            }
        }

        ImGui::EndChild();
    }
}

void drawBehaviorTreeGraphEditorWindow()
{
    if (s_state.nodes.empty())
    {
        GraphNode root;
        root.id = s_state.nextNodeId++;
        root.type = BehaviorTreeRuntime::NodeType::Sequence;
        s_state.rootId = root.id;
        s_state.selectedId = root.id;
        s_state.nodes.push_back(root);
    }

    ImGui::InputText("BT File", s_state.filePath, IM_ARRAYSIZE(s_state.filePath));

    if (ImGui::Button("Load"))
    {
        if (loadGraphFromFile(std::filesystem::path(s_state.filePath)))
        {
            s_state.status = "Load success";
        }
        else
        {
            s_state.status = "Load failed";
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        if (saveGraphToFile(std::filesystem::path(s_state.filePath)))
        {
            s_state.status = "Save success";
        }
        else
        {
            s_state.status = "Save failed (check root/node data).";
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Add Node"))
    {
        GraphNode node;
        node.id = s_state.nextNodeId++;
        node.type = BehaviorTreeRuntime::NodeType::Sequence;
        node.position = ImVec2(60.0f + static_cast<float>(s_state.nodes.size() % 4) * 180.0f,
            60.0f + static_cast<float>(s_state.nodes.size() / 4) * 120.0f);
        s_state.nodes.push_back(node);
        s_state.selectedId = node.id;
    }

    ImGui::SameLine();
    if (s_state.selectedId >= 0 && ImGui::Button("Remove Selected"))
    {
        removeNode(s_state.selectedId);
    }

    if (s_state.selectedId >= 0)
    {
        GraphNode* node = findNodeById(s_state.selectedId);
        if (node)
        {
            int typeIndex = static_cast<int>(node->type);
            const char* typeItems[] =
            {
                "Selector",
                "Sequence",
                "ConditionHasDestination",
                "ConditionHasPath",
                "ActionMoveToDestination",
                "ActionStop"
            };

            ImGui::Text("Selected Node: %d", node->id);
            ImGui::Combo("Type", &typeIndex, typeItems, IM_ARRAYSIZE(typeItems));
            node->type = static_cast<BehaviorTreeRuntime::NodeType>(typeIndex);

            if (ImGui::Button("Set As Root"))
            {
                s_state.rootId = node->id;
            }
            ImGui::SameLine();
            ImGui::Text("Current Root: %d", s_state.rootId);

            if (ImGui::BeginCombo("Add Child", "Select target node"))
            {
                for (const GraphNode& candidate : s_state.nodes)
                {
                    if (candidate.id == node->id)
                    {
                        continue;
                    }

                    const bool selected = (s_state.pendingChildTarget == candidate.id);
                    if (ImGui::Selectable(std::format("#{} {}", candidate.id, nodeTypeName(candidate.type)).c_str(), selected))
                    {
                        s_state.pendingChildTarget = candidate.id;
                    }
                }
                ImGui::EndCombo();
            }

            if (s_state.pendingChildTarget >= 0 && ImGui::Button("Connect Child"))
            {
                if (std::find(node->children.begin(), node->children.end(), s_state.pendingChildTarget) == node->children.end())
                {
                    node->children.push_back(s_state.pendingChildTarget);
                }
            }

            if (!node->children.empty())
            {
                ImGui::Text("Children");
                for (size_t i = 0; i < node->children.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::Text("%d", node->children[i]);
                    ImGui::SameLine();
                    if (ImGui::Button("Remove"))
                    {
                        node->children.erase(node->children.begin() + static_cast<std::ptrdiff_t>(i));
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
            }
        }
    }

    drawGraphCanvas();

    if (!s_state.status.empty())
    {
        ImGui::TextWrapped("%s", s_state.status.c_str());
    }
}
