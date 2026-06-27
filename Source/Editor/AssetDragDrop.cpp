#include "pch.h"
#include "AssetDragDrop.h"

#include "AsyncAssetLoader.h"
#include "EditorContext.h"
#include "GameObject/GameObject.h"

namespace
{
    constexpr const char* kAssetPayloadType = "DND_ASSET_PATH";

    enum class AssetType
    {
        Prefab,
        Scene,
        Fbx,
        Unsupported
    };

    std::string pathToUtf8(const std::filesystem::path& path)
    {
        const std::u8string u8 = path.generic_u8string();
        return std::string(u8.begin(), u8.end());
    }

    std::string toLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::filesystem::path fromUtf8Path(std::string_view utf8)
    {
        std::u8string u8;
        u8.reserve(utf8.size());
        for (char ch : utf8)
        {
            u8.push_back(static_cast<char8_t>(static_cast<unsigned char>(ch)));
        }
        return std::filesystem::path(u8);
    }

    std::filesystem::path normalizePath(const std::filesystem::path& source)
    {
        std::error_code ec;
        const std::filesystem::path absolute = std::filesystem::weakly_canonical(source, ec);
        if (!ec)
        {
            return absolute;
        }

        return source.lexically_normal();
    }

    std::string toAssetPathString(const std::filesystem::path& source)
    {
        const std::filesystem::path normalized = normalizePath(source);
        std::error_code ec;
        const std::filesystem::path relative = std::filesystem::relative(normalized, std::filesystem::current_path(), ec);
        if (!ec && !relative.empty())
        {
            return pathToUtf8(relative.lexically_normal());
        }

        return pathToUtf8(normalized);
    }

    AssetType detectAssetType(const std::filesystem::path& filePath)
    {
        const std::string ext = toLower(filePath.extension().string());
        if (ext == ".prefab")
        {
            return AssetType::Prefab;
        }

        if (ext == ".scn")
        {
            return AssetType::Scene;
        }

        if (ext == ".fbx")
        {
            return AssetType::Fbx;
        }

        return AssetType::Unsupported;
    }
}

namespace EditorAssetDragDrop
{
    const char* getAssetPayloadType()
    {
        return kAssetPayloadType;
    }

    bool beginAssetDragSource(const std::filesystem::path& assetPath, const char* label)
    {
        if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            return false;
        }

        const std::string payloadPath = toAssetPathString(assetPath);
        ImGui::SetDragDropPayload(kAssetPayloadType, payloadPath.c_str(), payloadPath.size() + 1);

        if (label && label[0] != '\0')
        {
            ImGui::Text("%s", label);
        }
        else
        {
            ImGui::Text("%s", payloadPath.c_str());
        }

        ImGui::EndDragDropSource();
        return true;
    }

    bool applyAssetToScene(const std::filesystem::path& assetPath, GameObject* parent)
    {
        const std::filesystem::path normalized = normalizePath(assetPath);
        switch (detectAssetType(normalized))
        {
        case AssetType::Prefab:
            return EditorAsyncAsset::AsyncAssetLoader::Instance().enqueuePrefab(normalized, parent);
        case AssetType::Scene:
            return EditorAsyncAsset::AsyncAssetLoader::Instance().enqueueScene(normalized);
        case AssetType::Fbx:
            return EditorAsyncAsset::AsyncAssetLoader::Instance().enqueueFbx(normalized, parent);
        default:
            break;
        }

        LOG_INFO("[AssetDragDrop] Unsupported asset type: %s", normalized.string().c_str());
        return false;
    }

    bool acceptAssetDropInCurrentTarget(GameObject* parent)
    {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetPayloadType);
        if (!payload || !payload->Data || payload->DataSize <= 1)
        {
            return false;
        }

        const char* payloadText = static_cast<const char*>(payload->Data);
        return applyAssetToScene(fromUtf8Path(std::string_view(payloadText)), parent);
    }
}