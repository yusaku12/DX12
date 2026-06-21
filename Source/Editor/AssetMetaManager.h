#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace EditorAssetMeta
{
    //! アセットメタ情報
    struct Meta
    {
        std::string guid;
        std::string importer;
        std::string thumbnailMode; //!< Auto / Icon / Texture
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
        void clearCache();

    private:
        AssetMetaManager() = default;
        ~AssetMetaManager() = default;

        AssetMetaManager(const AssetMetaManager&) = delete;
        AssetMetaManager& operator=(const AssetMetaManager&) = delete;

        Meta loadOrCreateMeta(const std::filesystem::path& assetPath);
        bool saveMeta(const std::filesystem::path& metaPath, const Meta& meta) const;

        std::unordered_map<std::filesystem::path, Meta> m_cache;
    };
}
