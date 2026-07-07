#pragma once

#include <cstdint>

class dtNavMesh;
class dtNavMeshQuery;

class NavMeshSystem
{
public:

    struct RaycastHit
    {
        Vector3 position = Vector3::Zero;
        bool blocked = false;
    };

    static NavMeshSystem& Instance()
    {
        static NavMeshSystem instance;
        return instance;
    }

    bool loadNavMesh(const std::filesystem::path& filePath);
    bool hasNavMesh() const { return m_navMesh != nullptr && m_navQuery != nullptr; }
    void clear();

    bool findPath(const Vector3& start, const Vector3& goal, std::vector<Vector3>& outPath) const;
    bool findNearestPoint(const Vector3& position, Vector3& outPoint) const;
    bool raycast(const Vector3& start, const Vector3& end, RaycastHit& outHit) const;

    const std::filesystem::path& getLoadedAssetPath() const { return m_loadedAssetPath; }

private:
    NavMeshSystem() = default;
    ~NavMeshSystem();

    NavMeshSystem(const NavMeshSystem&) = delete;
    NavMeshSystem(NavMeshSystem&&) = delete;
    NavMeshSystem& operator=(const NavMeshSystem&) = delete;
    NavMeshSystem& operator=(NavMeshSystem&&) = delete;

    struct ParsedNavGraph
    {
        uint32_t version = 1;
        std::vector<Vector3> nodes;
        std::vector<std::pair<uint32_t, uint32_t>> edges;
    };

    bool buildDetourNavMesh(const ParsedNavGraph& graph, std::string& errorMessage);

    static constexpr int kMaxPathPolys = 2048;
    static constexpr int kMaxStraightPathPoints = 2048;

    dtNavMesh* m_navMesh = nullptr;
    dtNavMeshQuery* m_navQuery = nullptr;
    float m_queryHalfExtents[3] = { 2.0f, 4.0f, 2.0f };
    std::filesystem::path m_loadedAssetPath;
};
