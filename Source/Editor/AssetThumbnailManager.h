#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace EditorAssetThumbnail
{
    //! サムネイル管理
    class AssetThumbnailManager
    {
    public:
        static AssetThumbnailManager& Instance()
        {
            static AssetThumbnailManager instance;
            return instance;
        }

        ImTextureID getThumbnail(
            const std::filesystem::path& assetPath,
            const std::string& importer,
            const std::string& thumbnailMode);

        void clear();

    private:
        AssetThumbnailManager() = default;
        ~AssetThumbnailManager() = default;

        AssetThumbnailManager(const AssetThumbnailManager&) = delete;
        AssetThumbnailManager& operator=(const AssetThumbnailManager&) = delete;

        ImTextureID getOrCreateIcon(const std::filesystem::path& assetPath, const std::string& importer);
        ImTextureID tryGetFbxTexturePreview(const std::filesystem::path& assetPath);

        struct IconCacheEntry
        {
            std::unique_ptr<LoadTexture> texture;
            std::filesystem::file_time_type lastWriteTime{};
        };

        struct FbxPreviewCacheEntry
        {
            ImTextureID textureId = ImTextureID_Invalid;
            std::filesystem::file_time_type lastWriteTime{};
        };

        std::unordered_map<std::filesystem::path, IconCacheEntry> m_iconCache;
        std::unordered_map<std::filesystem::path, FbxPreviewCacheEntry> m_fbxPreviewCache;
    };
}
