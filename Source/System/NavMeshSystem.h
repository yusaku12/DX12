#pragma once

#include <cstdint>

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
    bool hasNavMesh() const { return !m_data.nodes.empty(); }
    void clear();

    bool findPath(const Vector3& start, const Vector3& goal, std::vector<Vector3>& outPath) const;
    bool findNearestPoint(const Vector3& position, Vector3& outPoint) const;
    bool raycast(const Vector3& start, const Vector3& end, RaycastHit& outHit) const;

    const std::filesystem::path& getLoadedAssetPath() const { return m_loadedAssetPath; }

private:

    struct Node
    {
        Vector3 position = Vector3::Zero;
        std::vector<uint32_t> neighbors;
    };

    struct NavMeshData
    {
        uint32_t version = 1;
        std::vector<Node> nodes;
    };

    NavMeshSystem() = default;
    ~NavMeshSystem() = default;

    NavMeshSystem(const NavMeshSystem&) = delete;
    NavMeshSystem(NavMeshSystem&&) = delete;
    NavMeshSystem& operator=(const NavMeshSystem&) = delete;
    NavMeshSystem& operator=(NavMeshSystem&&) = delete;

    int findNearestNodeIndex(const Vector3& position) const;

    NavMeshData m_data;
    std::filesystem::path m_loadedAssetPath;
};
