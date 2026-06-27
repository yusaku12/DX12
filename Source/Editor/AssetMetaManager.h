#pragma once

#include <filesystem>
#include <cstdint>
#include <unordered_set>
#include <string>
#include <vector>
#include <unordered_map>

namespace EditorAssetMeta
{
    //! アセットメタ情報
    struct Meta
    {
        uint32_t schemaVersion = 2;
        std::string guid;
        std::string importer;
        uint32_t importerVersion = 1;
        std::string thumbnailMode; //!< Auto / Icon / Texture
        uint64_t sourceHash = 0;
        uint64_t dependencyHash = 0;
        uint64_t lastImportedHash = 0;
        std::vector<std::string> dependencies;
    };

    struct ReimportReport
    {
        bool schemaMigrated = false;
        bool sourceChanged = false;
        bool dependencyChanged = false;
        bool importerVersionChanged = false;
        bool reimported = false;
        uint64_t combinedHash = 0;
        std::vector<std::filesystem::path> dependencies;
    };

    struct CookReport
    {
        size_t cookedAssetCount = 0;
        size_t copiedFileCount = 0;
        std::filesystem::path manifestPath;
    };

    //! メタ管理
    class AssetMetaManager
    {
    public:
        static AssetMetaManager& Instance()
        {
            static AssetMetaManager instance;
            return instance;
        }

        const Meta& getOrCreate(const std::filesystem::path& assetPath);
        bool refreshAsset(const std::filesystem::path& assetPath, ReimportReport* outReport = nullptr);
        bool refreshAllAssets(const std::filesystem::path& rootPath = std::filesystem::path("Data"));
        bool cookAssets(const std::filesystem::path& sourceRoot,
            const std::filesystem::path& cookRoot,
            CookReport* outReport = nullptr);

        std::vector<std::filesystem::path> getDependencies(const std::filesystem::path& assetPath) const;
        std::vector<std::filesystem::path> getDependents(const std::filesystem::path& assetPath) const;

        static constexpr uint32_t kCurrentMetaSchemaVersion = 2;
        static constexpr uint32_t kCurrentManifestVersion = 1;

        void clearCache();

    private:
        AssetMetaManager() = default;
        ~AssetMetaManager() = default;

        AssetMetaManager(const AssetMetaManager&) = delete;
        AssetMetaManager& operator=(const AssetMetaManager&) = delete;

        Meta loadOrCreateMeta(const std::filesystem::path& assetPath);
        bool saveMeta(const std::filesystem::path& metaPath, const Meta& meta) const;
        void updateGraph(const std::filesystem::path& assetPath, const Meta& meta);

        std::unordered_map<std::filesystem::path, Meta> m_cache;
        std::unordered_map<std::filesystem::path, std::unordered_set<std::filesystem::path>> m_dependencies;
        std::unordered_map<std::filesystem::path, std::unordered_set<std::filesystem::path>> m_dependents;
    };
}
