#pragma once

#include "Editor/AssetMetaManager.h"

namespace EditorAssetPipeline
{
    enum class TextureTarget
    {
        CopySource,
        BC7,
        ExternalCommand,
    };

    struct TextureCookSettings
    {
        TextureTarget target = TextureTarget::BC7;
        std::wstring externalEncoderPath;
        std::wstring externalEncoderArguments;
        std::wstring outputExtension = L".dds";
    };

    struct CookSettings
    {
        std::filesystem::path sourceRoot = std::filesystem::path("Data");
        std::filesystem::path cookRoot = std::filesystem::path("CookedData") / "Windows";
        TextureCookSettings texture;
    };

    struct CookReport
    {
        size_t assetCount = 0;
        size_t textureCount = 0;
        size_t convertedTextureCount = 0;
        size_t copiedFileCount = 0;
        std::filesystem::path manifestPath;
    };

    struct PackageReport
    {
        size_t entryCount = 0;
        uint64_t payloadBytes = 0;
        uint64_t pakHash = 0;
        std::filesystem::path pakPath;
        std::filesystem::path manifestPath;
    };

    struct PatchReport
    {
        size_t changedCount = 0;
        size_t removedCount = 0;
        uint64_t patchHash = 0;
        std::filesystem::path patchPath;
        std::filesystem::path manifestPath;
    };

    class AssetPipelineManager
    {
    public:
        static AssetPipelineManager& Instance()
        {
            static AssetPipelineManager instance;
            return instance;
        }

        bool cookAssets(const CookSettings& settings, CookReport* outReport = nullptr);
        bool buildPak(const std::filesystem::path& cookedRoot, const std::filesystem::path& pakPath, PackageReport* outReport = nullptr);
        bool buildPatch(const std::filesystem::path& basePakPath, const std::filesystem::path& newPakPath, const std::filesystem::path& patchPath, PatchReport* outReport = nullptr);

    private:
        AssetPipelineManager() = default;
        ~AssetPipelineManager() = default;

        AssetPipelineManager(const AssetPipelineManager&) = delete;
        AssetPipelineManager& operator=(const AssetPipelineManager&) = delete;
    };
}