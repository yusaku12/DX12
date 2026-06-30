#include "pch.h"
#include "AssetThumbnailManager.h"
#include "Model/FbxLoad.h"

namespace
{
    std::filesystem::path normalizePathKey(const std::filesystem::path& path)
    {
        std::error_code ec;
        const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
        if (!ec)
        {
            return canonical;
        }

        if (path.is_absolute())
        {
            return path.lexically_normal();
        }

        return (std::filesystem::current_path() / path).lexically_normal();
    }

    ImTextureID toImTextureID(D3D12_GPU_DESCRIPTOR_HANDLE handle)
    {
        return static_cast<ImTextureID>(handle.ptr);
    }

    std::string toLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    bool equalsIgnoreCase(const std::string& a, const std::string& b)
    {
        return toLower(a) == toLower(b);
    }

    std::wstring pathToWstring(const std::filesystem::path& path)
    {
        return path.wstring();
    }

    bool fileExists(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::exists(path, ec) && !ec;
    }

    Vector3 pickBaseColor(const std::filesystem::path& assetPath, const std::string& importer)
    {
        const std::string ext = toLower(assetPath.extension().string());
        if (ext == ".prefab") return Vector3(0.86f, 0.62f, 0.18f);
        if (ext == ".fbx") return Vector3(0.22f, 0.62f, 0.87f);
        if (ext == ".scn") return Vector3(0.22f, 0.75f, 0.32f);
        if (equalsIgnoreCase(importer, "TextureImporter")) return Vector3(0.62f, 0.34f, 0.82f);
        return Vector3(0.45f, 0.45f, 0.45f);
    }

    std::vector<uint8_t> buildIconPixels(const std::filesystem::path& assetPath, const std::string& importer, UINT size)
    {
        std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);

        const Vector3 base = pickBaseColor(assetPath, importer);

        for (UINT y = 0; y < size; ++y)
        {
            for (UINT x = 0; x < size; ++x)
            {
                const float fx = static_cast<float>(x) / static_cast<float>(size - 1);
                const float fy = static_cast<float>(y) / static_cast<float>(size - 1);
                const float g = 0.55f + 0.45f * (0.6f * fx + 0.4f * (1.0f - fy));

                const size_t idx = (static_cast<size_t>(y) * size + x) * 4;
                pixels[idx + 0] = static_cast<uint8_t>(std::clamp(base.x * g * 255.0f, 0.0f, 255.0f));
                pixels[idx + 1] = static_cast<uint8_t>(std::clamp(base.y * g * 255.0f, 0.0f, 255.0f));
                pixels[idx + 2] = static_cast<uint8_t>(std::clamp(base.z * g * 255.0f, 0.0f, 255.0f));
                pixels[idx + 3] = 255;
            }
        }

        // 枠線
        for (UINT i = 0; i < size; ++i)
        {
            const size_t top = static_cast<size_t>(i) * 4;
            const size_t bottom = (static_cast<size_t>(size - 1) * size + i) * 4;
            const size_t left = (static_cast<size_t>(i) * size) * 4;
            const size_t right = (static_cast<size_t>(i) * size + (size - 1)) * 4;
            pixels[top + 0] = pixels[top + 1] = pixels[top + 2] = 20;
            pixels[bottom + 0] = pixels[bottom + 1] = pixels[bottom + 2] = 20;
            pixels[left + 0] = pixels[left + 1] = pixels[left + 2] = 20;
            pixels[right + 0] = pixels[right + 1] = pixels[right + 2] = 20;
        }

        return pixels;
    }
}

namespace EditorAssetThumbnail
{
    ImTextureID AssetThumbnailManager::getThumbnail(
        const std::filesystem::path& assetPath,
        const std::string& importer,
        const std::string& thumbnailMode)
    {
        const std::string ext = toLower(assetPath.extension().string());
        const bool allowTexturePreview = !equalsIgnoreCase(thumbnailMode, "Icon") && equalsIgnoreCase(importer, "TextureImporter") && ext != ".dds";

        if (equalsIgnoreCase(importer, "FbxImporter"))
        {
            ImTextureID texID = tryGetFbxTexturePreview(assetPath);
            if (texID != ImTextureID_Invalid)
            {
                return texID;
            }
        }

        if (allowTexturePreview)
        {
            TextureManager::Instance().requestStreaming(assetPath.wstring(), TextureManager::StreamPriority::Low);
            LoadTexture* texture = TextureManager::Instance().findCached(assetPath.wstring());
            if (texture)
            {
                const UINT srv = texture->getSRVIndex();
                if (srv != UINT_MAX)
                {
                    const auto gpu = DescriptorHeapManager::Instance().getGPUHandle(srv);
                    return toImTextureID(gpu);
                }
            }
        }

        return getOrCreateIcon(assetPath, importer);
    }

    ImTextureID AssetThumbnailManager::tryGetFbxTexturePreview(const std::filesystem::path& assetPath)
    {
        const std::filesystem::path normalizedPath = normalizePathKey(assetPath);

        std::error_code timeEc;
        const std::filesystem::file_time_type lastWriteTime = std::filesystem::last_write_time(normalizedPath, timeEc);
        auto cacheIt = m_fbxPreviewCache.find(normalizedPath);
        if (cacheIt != m_fbxPreviewCache.end() && (timeEc || cacheIt->second.lastWriteTime == lastWriteTime))
        {
            return cacheIt->second.textureId;
        }

        FbxLoad loader;
        const std::string fbxPathNarrow = normalizedPath.string();
        if (!loader.load(fbxPathNarrow.c_str()))
        {
            m_fbxPreviewCache[normalizedPath] = { ImTextureID_Invalid, lastWriteTime };
            return ImTextureID_Invalid;
        }

        const auto& model = loader.getModelData();
        for (const auto& material : model.materials)
        {
            const std::string& texName = material.textureName[static_cast<size_t>(TextureType::Diffuse)];
            if (texName.empty())
            {
                continue;
            }

            std::filesystem::path texturePath(texName);
            if (!texturePath.is_absolute())
            {
                texturePath = normalizedPath.parent_path() / texturePath;
            }
            texturePath = normalizePathKey(texturePath);

            if (!fileExists(texturePath))
            {
                continue;
            }

            TextureManager::Instance().requestStreaming(pathToWstring(texturePath), TextureManager::StreamPriority::Normal);
            LoadTexture* texture = TextureManager::Instance().findCached(pathToWstring(texturePath));
            if (!texture)
            {
                continue;
            }

            const UINT srv = texture->getSRVIndex();
            if (srv == UINT_MAX)
            {
                continue;
            }

            const auto gpu = DescriptorHeapManager::Instance().getGPUHandle(srv);
            const ImTextureID texId = toImTextureID(gpu);
            m_fbxPreviewCache[normalizedPath] = { texId, lastWriteTime };
            return texId;
        }

        m_fbxPreviewCache[normalizedPath] = { ImTextureID_Invalid, lastWriteTime };
        return ImTextureID_Invalid;
    }

    void AssetThumbnailManager::clear()
    {
        m_iconCache.clear();
        m_fbxPreviewCache.clear();
    }

    ImTextureID AssetThumbnailManager::getOrCreateIcon(const std::filesystem::path& assetPath, const std::string& importer)
    {
        const std::filesystem::path key = normalizePathKey(assetPath);

        std::error_code ec;
        const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(key, ec);

        auto found = m_iconCache.find(key);
        if (found != m_iconCache.end())
        {
            if (ec || found->second.lastWriteTime == writeTime)
            {
                if (found->second.texture)
                {
                    const UINT srv = found->second.texture->getSRVIndex();
                    if (srv != UINT_MAX)
                    {
                        const auto gpu = DescriptorHeapManager::Instance().getGPUHandle(srv);
                        return toImTextureID(gpu);
                    }
                }
            }
        }

        constexpr UINT kThumbSize = 96;
        std::vector<uint8_t> pixels = buildIconPixels(assetPath, importer, kThumbSize);

        IconCacheEntry entry;
        entry.texture = DXMem::makeUnique<LoadTexture>(kThumbSize, kThumbSize, DXGI_FORMAT_R8G8B8A8_UNORM, pixels.data(), pixels.size());
        entry.lastWriteTime = writeTime;

        LoadTexture* texture = entry.texture.get();
        m_iconCache[key] = std::move(entry);

        if (!texture)
        {
            return ImTextureID_Invalid;
        }

        const UINT srv = texture->getSRVIndex();
        if (srv == UINT_MAX)
        {
            return ImTextureID_Invalid;
        }

        const auto gpu = DescriptorHeapManager::Instance().getGPUHandle(srv);
        return toImTextureID(gpu);
    }
}