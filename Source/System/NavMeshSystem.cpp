#include "pch.h"
#include "NavMeshSystem.h"

namespace
{
    constexpr uint32_t kSupportedNavMeshVersion = 1;

    float sqDistance(const Vector3& a, const Vector3& b)
    {
        return (a - b).LengthSquared();
    }
}

bool NavMeshSystem::loadNavMesh(const std::filesystem::path& filePath)
{
    std::ifstream in(filePath);
    if (!in)
    {
        clear();
        LOG_ERROR("[NavMeshSystem] Failed to open navmesh: %s", filePath.string().c_str());
        return false;
    }

    std::string token;
    NavMeshData nextData;
    uint32_t nodeCount = 0;

    while (in >> token)
    {
        if (token == "version")
        {
            in >> nextData.version;
        }
        else if (token == "nodes")
        {
            in >> nodeCount;
            nextData.nodes.assign(nodeCount, Node{});
        }
        else if (token == "node")
        {
            uint32_t index = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            in >> index >> x >> y >> z;
            if (index < nextData.nodes.size())
            {
                nextData.nodes[index].position = Vector3(x, y, z);
            }
        }
        else if (token == "edge")
        {
            uint32_t from = 0;
            uint32_t to = 0;
            in >> from >> to;
            if (from < nextData.nodes.size() && to < nextData.nodes.size())
            {
                nextData.nodes[from].neighbors.push_back(to);
                nextData.nodes[to].neighbors.push_back(from);
            }
        }
    }

    if (nextData.version != kSupportedNavMeshVersion)
    {
        clear();
        LOG_ERROR("[NavMeshSystem] Unsupported navmesh version: %u file=%s", nextData.version, filePath.string().c_str());
        return false;
    }

    if (nextData.nodes.empty())
    {
        clear();
        LOG_ERROR("[NavMeshSystem] Navmesh has no nodes: %s", filePath.string().c_str());
        return false;
    }

    m_data = std::move(nextData);
    m_loadedAssetPath = filePath;
    LOG_INFO("[NavMeshSystem] Loaded navmesh: %s nodes=%zu", filePath.string().c_str(), m_data.nodes.size());
    return true;
}

void NavMeshSystem::clear()
{
    m_data = {};
    m_loadedAssetPath.clear();
}

bool NavMeshSystem::findPath(const Vector3& start, const Vector3& goal, std::vector<Vector3>& outPath) const
{
    outPath.clear();
    if (m_data.nodes.empty())
    {
        return false;
    }

    const int startIndex = findNearestNodeIndex(start);
    const int goalIndex = findNearestNodeIndex(goal);
    if (startIndex < 0 || goalIndex < 0)
    {
        return false;
    }

    struct NodeState
    {
        float g = FLT_MAX;
        float f = FLT_MAX;
        int parent = -1;
        bool closed = false;
    };

    std::vector<NodeState> states(m_data.nodes.size());
    std::vector<uint32_t> open;
    open.reserve(m_data.nodes.size());

    states[static_cast<size_t>(startIndex)].g = 0.0f;
    states[static_cast<size_t>(startIndex)].f = std::sqrt(sqDistance(m_data.nodes[static_cast<size_t>(startIndex)].position, m_data.nodes[static_cast<size_t>(goalIndex)].position));
    open.push_back(static_cast<uint32_t>(startIndex));

    auto popBest = [&]() -> int
    {
        if (open.empty())
        {
            return -1;
        }

        size_t bestPos = 0;
        float bestF = states[open[0]].f;
        for (size_t i = 1; i < open.size(); ++i)
        {
            const float f = states[open[i]].f;
            if (f < bestF)
            {
                bestF = f;
                bestPos = i;
            }
        }

        const uint32_t nodeIndex = open[bestPos];
        open[bestPos] = open.back();
        open.pop_back();
        return static_cast<int>(nodeIndex);
    };

    bool found = false;
    while (!open.empty())
    {
        const int current = popBest();
        if (current < 0)
        {
            break;
        }

        if (states[static_cast<size_t>(current)].closed)
        {
            continue;
        }

        states[static_cast<size_t>(current)].closed = true;
        if (current == goalIndex)
        {
            found = true;
            break;
        }

        const Vector3 currentPos = m_data.nodes[static_cast<size_t>(current)].position;
        for (uint32_t neighbor : m_data.nodes[static_cast<size_t>(current)].neighbors)
        {
            if (neighbor >= states.size() || states[neighbor].closed)
            {
                continue;
            }

            const float edgeCost = std::sqrt(sqDistance(currentPos, m_data.nodes[neighbor].position));
            const float tentativeG = states[static_cast<size_t>(current)].g + edgeCost;
            if (tentativeG >= states[neighbor].g)
            {
                continue;
            }

            states[neighbor].parent = current;
            states[neighbor].g = tentativeG;
            states[neighbor].f = tentativeG + std::sqrt(sqDistance(m_data.nodes[neighbor].position, m_data.nodes[static_cast<size_t>(goalIndex)].position));
            open.push_back(neighbor);
        }
    }

    if (!found)
    {
        return false;
    }

    std::vector<Vector3> reversePath;
    reversePath.reserve(m_data.nodes.size());
    int current = goalIndex;
    while (current >= 0)
    {
        reversePath.push_back(m_data.nodes[static_cast<size_t>(current)].position);
        current = states[static_cast<size_t>(current)].parent;
    }

    outPath.reserve(reversePath.size() + 2);
    outPath.push_back(start);
    for (auto it = reversePath.rbegin(); it != reversePath.rend(); ++it)
    {
        outPath.push_back(*it);
    }
    outPath.push_back(goal);

    return true;
}

bool NavMeshSystem::findNearestPoint(const Vector3& position, Vector3& outPoint) const
{
    const int index = findNearestNodeIndex(position);
    if (index < 0)
    {
        return false;
    }

    outPoint = m_data.nodes[static_cast<size_t>(index)].position;
    return true;
}

bool NavMeshSystem::raycast(const Vector3& start, const Vector3& end, RaycastHit& outHit) const
{
    if (m_data.nodes.empty())
    {
        return false;
    }

    outHit.position = end;
    outHit.blocked = false;
    (void)start;
    return true;
}

int NavMeshSystem::findNearestNodeIndex(const Vector3& position) const
{
    if (m_data.nodes.empty())
    {
        return -1;
    }

    int bestIndex = 0;
    float bestDistance = sqDistance(position, m_data.nodes[0].position);
    for (size_t i = 1; i < m_data.nodes.size(); ++i)
    {
        const float distance = sqDistance(position, m_data.nodes[i].position);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = static_cast<int>(i);
        }
    }

    return bestIndex;
}
