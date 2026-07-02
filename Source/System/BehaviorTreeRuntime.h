#pragma once

#include <variant>

class GameObject;

class BehaviorTreeRuntime
{
public:

    enum class NodeType
    {
        Selector,
        Sequence,
        ConditionHasDestination,
        ConditionHasPath,
        ActionMoveToDestination,
        ActionStop
    };

    enum class NodeResult
    {
        Success,
        Failure,
        Running
    };

    struct Node
    {
        int id = -1;
        NodeType type = NodeType::Sequence;
        std::vector<int> children;
    };

    struct Asset
    {
        std::vector<Node> nodes;
        int rootNodeId = -1;
    };

    struct Context
    {
        std::unordered_map<std::string, std::variant<bool, int, float, Vector3, uint64_t>> blackboard;
    };

    static bool loadAsset(const std::filesystem::path& filePath, Asset& outAsset);
    static bool tick(const Asset& asset, GameObject* owner, Context& context, float deltaTime);

private:

    static NodeResult tickNode(const Asset& asset, int nodeId, GameObject* owner, Context& context, float deltaTime);
    static const Node* findNode(const Asset& asset, int nodeId);
};
