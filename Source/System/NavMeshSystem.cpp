#include "pch.h"
#include "NavMeshSystem.h"

#include <DetourAlloc.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>

namespace
{
    constexpr uint32_t kSupportedNavMeshVersion = 1;

    void toDtVector(const Vector3& v, float out[3])
    {
        out[0] = v.x;
        out[1] = v.y;
        out[2] = v.z;
    }

    Vector3 fromDtVector(const float v[3])
    {
        return Vector3(v[0], v[1], v[2]);
    }

    uint64_t makeEdgeKey(uint32_t a, uint32_t b)
    {
        const uint32_t lo = std::min(a, b);
        const uint32_t hi = std::max(a, b);
        return (static_cast<uint64_t>(lo) << 32) | static_cast<uint64_t>(hi);
    }
}

NavMeshSystem::~NavMeshSystem()
{
    clear();
}

bool NavMeshSystem::loadNavMesh(const std::filesystem::path& filePath)
{
    std::ifstream in(filePath);
    if (!in.is_open())
    {
        LOG_WARN("[NavMeshSystem] Failed to open navmesh file: %s", filePath.string().c_str());
        return false;
    }

    std::string token;
    ParsedNavGraph graph;
    uint32_t nodeCount = 0;
    std::unordered_set<uint64_t> edgeSet;

    while (in >> token)
    {
        if (token == "version")
        {
            in >> graph.version;
        }
        else if (token == "nodes")
        {
            in >> nodeCount;
            graph.nodes.assign(nodeCount, Vector3::Zero);
        }
        else if (token == "node")
        {
            uint32_t index = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            in >> index >> x >> y >> z;
            if (index < graph.nodes.size())
            {
                graph.nodes[index] = Vector3(x, y, z);
            }
        }
        else if (token == "edge")
        {
            uint32_t from = 0;
            uint32_t to = 0;
            in >> from >> to;
            if (from < graph.nodes.size() && to < graph.nodes.size() && from != to)
            {
                const uint64_t key = makeEdgeKey(from, to);
                if (edgeSet.insert(key).second)
                {
                    graph.edges.emplace_back(from, to);
                }
            }
        }
    }

    if (graph.version != kSupportedNavMeshVersion)
    {
        clear();
        LOG_ERROR("[NavMeshSystem] Unsupported navmesh version: %u file=%s", graph.version, filePath.string().c_str());
        return false;
    }

    if (graph.nodes.empty())
    {
        clear();
        LOG_ERROR("[NavMeshSystem] Navmesh has no nodes: %s", filePath.string().c_str());
        return false;
    }

    std::string errorMessage;
    if (!buildDetourNavMesh(graph, errorMessage))
    {
        clear();
        LOG_ERROR("[NavMeshSystem] Failed to build Detour navmesh: %s file=%s", errorMessage.c_str(), filePath.string().c_str());
        return false;
    }

    m_loadedAssetPath = filePath;
    LOG_INFO("[NavMeshSystem] Loaded navmesh(Detour): %s nodes=%zu edges=%zu", filePath.string().c_str(), graph.nodes.size(), graph.edges.size());
    return true;
}

void NavMeshSystem::clear()
{
    if (m_navQuery)
    {
        dtFreeNavMeshQuery(m_navQuery);
        m_navQuery = nullptr;
    }

    if (m_navMesh)
    {
        dtFreeNavMesh(m_navMesh);
        m_navMesh = nullptr;
    }

    m_loadedAssetPath.clear();
}

bool NavMeshSystem::findPath(const Vector3& start, const Vector3& goal, std::vector<Vector3>& outPath) const
{
    outPath.clear();
    if (!m_navMesh || !m_navQuery)
    {
        return false;
    }

    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);

    float startPos[3] = {};
    float goalPos[3] = {};
    float nearestStart[3] = {};
    float nearestGoal[3] = {};
    toDtVector(start, startPos);
    toDtVector(goal, goalPos);

    dtPolyRef startRef = 0;
    dtPolyRef goalRef = 0;

    if (dtStatusFailed(m_navQuery->findNearestPoly(startPos, m_queryHalfExtents, &filter, &startRef, nearestStart)) || startRef == 0)
    {
        return false;
    }

    if (dtStatusFailed(m_navQuery->findNearestPoly(goalPos, m_queryHalfExtents, &filter, &goalRef, nearestGoal)) || goalRef == 0)
    {
        return false;
    }

    std::array<dtPolyRef, kMaxPathPolys> corridor{};
    int corridorCount = 0;
    if (dtStatusFailed(m_navQuery->findPath(
        startRef,
        goalRef,
        nearestStart,
        nearestGoal,
        &filter,
        corridor.data(),
        &corridorCount,
        static_cast<int>(corridor.size()))) || corridorCount <= 0)
    {
        return false;
    }

    std::array<float, kMaxStraightPathPoints * 3> straightPath{};
    std::array<unsigned char, kMaxStraightPathPoints> straightFlags{};
    std::array<dtPolyRef, kMaxStraightPathPoints> straightRefs{};
    int straightCount = 0;

    if (dtStatusFailed(m_navQuery->findStraightPath(
        nearestStart,
        nearestGoal,
        corridor.data(),
        corridorCount,
        straightPath.data(),
        straightFlags.data(),
        straightRefs.data(),
        &straightCount,
        static_cast<int>(straightRefs.size()),
        DT_STRAIGHTPATH_ALL_CROSSINGS)) || straightCount <= 0)
    {
        return false;
    }

    outPath.reserve(static_cast<size_t>(straightCount));
    for (int i = 0; i < straightCount; ++i)
    {
        const float* p = &straightPath[static_cast<size_t>(i) * 3];
        outPath.push_back(Vector3(p[0], p[1], p[2]));
    }

    if (!outPath.empty())
    {
        outPath.front() = start;
        outPath.back() = goal;
    }

    return !outPath.empty();
}

bool NavMeshSystem::findNearestPoint(const Vector3& position, Vector3& outPoint) const
{
    if (!m_navMesh || !m_navQuery)
    {
        return false;
    }

    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);

    float center[3] = {};
    float nearest[3] = {};
    toDtVector(position, center);

    dtPolyRef nearestRef = 0;
    if (dtStatusFailed(m_navQuery->findNearestPoly(center, m_queryHalfExtents, &filter, &nearestRef, nearest)) || nearestRef == 0)
    {
        return false;
    }

    outPoint = fromDtVector(nearest);
    return true;
}

bool NavMeshSystem::raycast(const Vector3& start, const Vector3& end, RaycastHit& outHit) const
{
    if (!m_navMesh || !m_navQuery)
    {
        return false;
    }

    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);

    float startPos[3] = {};
    float endPos[3] = {};
    float nearestStart[3] = {};
    toDtVector(start, startPos);
    toDtVector(end, endPos);

    dtPolyRef startRef = 0;
    if (dtStatusFailed(m_navQuery->findNearestPoly(startPos, m_queryHalfExtents, &filter, &startRef, nearestStart)) || startRef == 0)
    {
        return false;
    }

    std::array<dtPolyRef, 256> rayPath{};
    dtRaycastHit hit{};
    hit.path = rayPath.data();
    hit.maxPath = static_cast<int>(rayPath.size());

    if (dtStatusFailed(m_navQuery->raycast(startRef, startPos, endPos, &filter, 0, &hit)))
    {
        return false;
    }

    if (hit.t >= 1.0f || hit.t == FLT_MAX)
    {
        outHit.position = end;
        outHit.blocked = false;
        return true;
    }

    const Vector3 direction = end - start;
    outHit.position = start + direction * hit.t;
    outHit.blocked = true;
    return true;
}

bool NavMeshSystem::buildDetourNavMesh(const ParsedNavGraph& graph, std::string& errorMessage)
{
    clear();

    if (graph.nodes.empty())
    {
        errorMessage = "graph has no nodes";
        return false;
    }

    constexpr int kVertsPerPoly = 4;
    constexpr float kNodeHalfExtent = 0.35f;
    constexpr float kOffMeshRadius = 0.6f;

    float minX = graph.nodes[0].x;
    float minY = graph.nodes[0].y;
    float minZ = graph.nodes[0].z;
    float maxX = graph.nodes[0].x;
    float maxY = graph.nodes[0].y;
    float maxZ = graph.nodes[0].z;

    for (const Vector3& node : graph.nodes)
    {
        minX = std::min(minX, node.x - kNodeHalfExtent);
        minY = std::min(minY, node.y - 1.0f);
        minZ = std::min(minZ, node.z - kNodeHalfExtent);
        maxX = std::max(maxX, node.x + kNodeHalfExtent);
        maxY = std::max(maxY, node.y + 1.0f);
        maxZ = std::max(maxZ, node.z + kNodeHalfExtent);
    }

    const float extentX = std::max(0.1f, maxX - minX);
    const float extentY = std::max(0.1f, maxY - minY);
    const float extentZ = std::max(0.1f, maxZ - minZ);
    const float maxExtent = std::max(extentX, extentZ);

    const float cs = std::max(0.05f, maxExtent / 60000.0f);
    const float ch = std::max(0.05f, extentY / 60000.0f);

    const int polyCount = static_cast<int>(graph.nodes.size());
    const int vertCount = polyCount * kVertsPerPoly;

    std::vector<unsigned short> verts(static_cast<size_t>(vertCount) * 3, 0);
    std::vector<unsigned short> polys(static_cast<size_t>(polyCount) * 2 * kVertsPerPoly, 0);
    std::vector<unsigned short> polyFlags(static_cast<size_t>(polyCount), 1);
    std::vector<unsigned char> polyAreas(static_cast<size_t>(polyCount), 0);

    auto encodeCoord = [](float value, float base, float scale) -> unsigned short
    {
        const float voxel = (value - base) / scale;
        const float clamped = std::clamp(voxel, 0.0f, 65535.0f);
        return static_cast<unsigned short>(std::lround(clamped));
    };

    for (int i = 0; i < polyCount; ++i)
    {
        const Vector3 center = graph.nodes[static_cast<size_t>(i)];
        const Vector3 corners[kVertsPerPoly] =
        {
            Vector3(center.x - kNodeHalfExtent, center.y, center.z - kNodeHalfExtent),
            Vector3(center.x + kNodeHalfExtent, center.y, center.z - kNodeHalfExtent),
            Vector3(center.x + kNodeHalfExtent, center.y, center.z + kNodeHalfExtent),
            Vector3(center.x - kNodeHalfExtent, center.y, center.z + kNodeHalfExtent)
        };

        for (int j = 0; j < kVertsPerPoly; ++j)
        {
            const int vertIndex = i * kVertsPerPoly + j;
            verts[static_cast<size_t>(vertIndex) * 3 + 0] = encodeCoord(corners[j].x, minX, cs);
            verts[static_cast<size_t>(vertIndex) * 3 + 1] = encodeCoord(corners[j].y, minY, ch);
            verts[static_cast<size_t>(vertIndex) * 3 + 2] = encodeCoord(corners[j].z, minZ, cs);

            polys[static_cast<size_t>(i) * 2 * kVertsPerPoly + j] = static_cast<unsigned short>(vertIndex);
            polys[static_cast<size_t>(i) * 2 * kVertsPerPoly + kVertsPerPoly + j] = 0;
        }
    }

    std::vector<float> offMeshConVerts;
    std::vector<float> offMeshConRad;
    std::vector<unsigned short> offMeshConFlags;
    std::vector<unsigned char> offMeshConAreas;
    std::vector<unsigned char> offMeshConDir;
    std::vector<unsigned int> offMeshConUserId;

    offMeshConVerts.reserve(graph.edges.size() * 6);
    offMeshConRad.reserve(graph.edges.size());
    offMeshConFlags.reserve(graph.edges.size());
    offMeshConAreas.reserve(graph.edges.size());
    offMeshConDir.reserve(graph.edges.size());
    offMeshConUserId.reserve(graph.edges.size());

    unsigned int edgeId = 1;
    for (const auto& edge : graph.edges)
    {
        if (edge.first >= graph.nodes.size() || edge.second >= graph.nodes.size())
        {
            continue;
        }

        const Vector3 a = graph.nodes[edge.first];
        const Vector3 b = graph.nodes[edge.second];
        offMeshConVerts.push_back(a.x);
        offMeshConVerts.push_back(a.y);
        offMeshConVerts.push_back(a.z);
        offMeshConVerts.push_back(b.x);
        offMeshConVerts.push_back(b.y);
        offMeshConVerts.push_back(b.z);

        offMeshConRad.push_back(kOffMeshRadius);
        offMeshConFlags.push_back(1);
        offMeshConAreas.push_back(0);
        offMeshConDir.push_back(DT_OFFMESH_CON_BIDIR);
        offMeshConUserId.push_back(edgeId++);
    }

    dtNavMeshCreateParams params{};
    params.verts = verts.data();
    params.vertCount = vertCount;
    params.polys = polys.data();
    params.polyFlags = polyFlags.data();
    params.polyAreas = polyAreas.data();
    params.polyCount = polyCount;
    params.nvp = kVertsPerPoly;
    params.offMeshConVerts = offMeshConVerts.empty() ? nullptr : offMeshConVerts.data();
    params.offMeshConRad = offMeshConRad.empty() ? nullptr : offMeshConRad.data();
    params.offMeshConFlags = offMeshConFlags.empty() ? nullptr : offMeshConFlags.data();
    params.offMeshConAreas = offMeshConAreas.empty() ? nullptr : offMeshConAreas.data();
    params.offMeshConDir = offMeshConDir.empty() ? nullptr : offMeshConDir.data();
    params.offMeshConUserID = offMeshConUserId.empty() ? nullptr : offMeshConUserId.data();
    params.offMeshConCount = static_cast<int>(offMeshConRad.size());
    params.walkableHeight = 2.0f;
    params.walkableRadius = 0.3f;
    params.walkableClimb = 1.0f;
    params.cs = cs;
    params.ch = ch;
    params.buildBvTree = true;
    params.tileX = 0;
    params.tileY = 0;
    params.tileLayer = 0;
    params.userId = 1;
    params.bmin[0] = minX;
    params.bmin[1] = minY;
    params.bmin[2] = minZ;
    params.bmax[0] = maxX;
    params.bmax[1] = maxY;
    params.bmax[2] = maxZ;

    unsigned char* navData = nullptr;
    int navDataSize = 0;
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize) || !navData || navDataSize <= 0)
    {
        errorMessage = "dtCreateNavMeshData failed";
        return false;
    }

    m_navMesh = dtAllocNavMesh();
    if (!m_navMesh)
    {
        dtFree(navData);
        errorMessage = "dtAllocNavMesh failed";
        return false;
    }

    if (dtStatusFailed(m_navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA)))
    {
        dtFree(navData);
        clear();
        errorMessage = "dtNavMesh::init failed";
        return false;
    }

    m_navQuery = dtAllocNavMeshQuery();
    if (!m_navQuery)
    {
        clear();
        errorMessage = "dtAllocNavMeshQuery failed";
        return false;
    }

    if (dtStatusFailed(m_navQuery->init(m_navMesh, 2048)))
    {
        clear();
        errorMessage = "dtNavMeshQuery::init failed";
        return false;
    }

    m_queryHalfExtents[0] = std::max(2.0f, extentX * 0.1f);
    m_queryHalfExtents[1] = std::max(4.0f, extentY * 0.5f + 1.0f);
    m_queryHalfExtents[2] = std::max(2.0f, extentZ * 0.1f);

    return true;
}
