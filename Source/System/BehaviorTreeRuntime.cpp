#include "pch.h"
#include "BehaviorTreeRuntime.h"

#include "Component/NavAgentComponent.h"
#include "GameObject/GameObject.h"

namespace
{
    bool parseNodeType(const std::string& text, BehaviorTreeRuntime::NodeType& outType)
    {
        if (text == "Selector") { outType = BehaviorTreeRuntime::NodeType::Selector; return true; }
        if (text == "Sequence") { outType = BehaviorTreeRuntime::NodeType::Sequence; return true; }
        if (text == "ConditionHasDestination") { outType = BehaviorTreeRuntime::NodeType::ConditionHasDestination; return true; }
        if (text == "ConditionHasPath") { outType = BehaviorTreeRuntime::NodeType::ConditionHasPath; return true; }
        if (text == "ActionMoveToDestination") { outType = BehaviorTreeRuntime::NodeType::ActionMoveToDestination; return true; }
        if (text == "ActionStop") { outType = BehaviorTreeRuntime::NodeType::ActionStop; return true; }
        return false;
    }
}

bool BehaviorTreeRuntime::loadAsset(const std::filesystem::path& filePath, Asset& outAsset)
{
    outAsset = {};

    std::ifstream in(filePath);
    if (!in)
    {
        LOG_ERROR("[BehaviorTreeRuntime] Failed to open tree: %s", filePath.string().c_str());
        return false;
    }

    std::unordered_map<int, size_t> idToIndex;

    std::string token;
    while (in >> token)
    {
        if (token == "root")
        {
            in >> outAsset.rootNodeId;
        }
        else if (token == "node")
        {
            int id = -1;
            std::string typeName;
            in >> id >> typeName;

            NodeType type = NodeType::Sequence;
            if (!parseNodeType(typeName, type))
            {
                LOG_WARN("[BehaviorTreeRuntime] Unknown node type: %s", typeName.c_str());
                continue;
            }

            Node node;
            node.id = id;
            node.type = type;
            idToIndex[id] = outAsset.nodes.size();
            outAsset.nodes.push_back(std::move(node));
        }
        else if (token == "child")
        {
            int parent = -1;
            int child = -1;
            in >> parent >> child;

            const auto it = idToIndex.find(parent);
            if (it != idToIndex.end())
            {
                outAsset.nodes[it->second].children.push_back(child);
            }
        }
    }

    if (outAsset.rootNodeId < 0 || outAsset.nodes.empty())
    {
        LOG_ERROR("[BehaviorTreeRuntime] Invalid tree asset: %s", filePath.string().c_str());
        return false;
    }

    return true;
}

bool BehaviorTreeRuntime::tick(const Asset& asset, GameObject* owner, Context& context, float deltaTime)
{
    if (!owner)
    {
        return false;
    }

    const NodeResult result = tickNode(asset, asset.rootNodeId, owner, context, deltaTime);
    return result != NodeResult::Failure;
}

BehaviorTreeRuntime::NodeResult BehaviorTreeRuntime::tickNode(const Asset& asset,
    int nodeId,
    GameObject* owner,
    Context& context,
    float deltaTime)
{
    const Node* node = findNode(asset, nodeId);
    if (!node)
    {
        return NodeResult::Failure;
    }

    NavAgentComponent* agent = owner->getComponent<NavAgentComponent>();

    switch (node->type)
    {
    case NodeType::Selector:
        for (int child : node->children)
        {
            const NodeResult childResult = tickNode(asset, child, owner, context, deltaTime);
            if (childResult == NodeResult::Success || childResult == NodeResult::Running)
            {
                return childResult;
            }
        }
        return NodeResult::Failure;

    case NodeType::Sequence:
        for (int child : node->children)
        {
            const NodeResult childResult = tickNode(asset, child, owner, context, deltaTime);
            if (childResult != NodeResult::Success)
            {
                return childResult;
            }
        }
        return NodeResult::Success;

    case NodeType::ConditionHasDestination:
        return (agent && agent->hasDestination()) ? NodeResult::Success : NodeResult::Failure;

    case NodeType::ConditionHasPath:
        return (agent && agent->hasPath()) ? NodeResult::Success : NodeResult::Failure;

    case NodeType::ActionMoveToDestination:
        if (!agent)
        {
            return NodeResult::Failure;
        }

        if (agent->hasDestination() && !agent->hasPath())
        {
            agent->requestRepath();
        }
        (void)context;
        (void)deltaTime;
        return agent->hasDestination() ? NodeResult::Running : NodeResult::Failure;

    case NodeType::ActionStop:
        if (!agent)
        {
            return NodeResult::Failure;
        }

        agent->stop();
        return NodeResult::Success;

    default:
        return NodeResult::Failure;
    }
}

const BehaviorTreeRuntime::Node* BehaviorTreeRuntime::findNode(const Asset& asset, int nodeId)
{
    for (const Node& node : asset.nodes)
    {
        if (node.id == nodeId)
        {
            return &node;
        }
    }

    return nullptr;
}
