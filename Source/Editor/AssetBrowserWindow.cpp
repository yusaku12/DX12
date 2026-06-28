#include "pch.h"
#include "AssetBrowserWindow.h"

#include "AsyncAssetLoader.h"
#include "AssetDragDrop.h"
#include "AssetMetaManager.h"
#include "AssetThumbnailManager.h"
#include "Scene/PrefabFlatBuffer.h"
#include "Scene/SceneManager.h"

namespace
{
    enum class AssetKind
    {
        Prefab,
        Scene,
        Fbx,
        Texture,
        Other
    };

    struct AssetEntry
    {
        std::filesystem::path absolutePath;
        std::filesystem::path relativePath;
        AssetKind kind = AssetKind::Other;
        std::string guid;
        std::string importer;
        std::string thumbnailMode;
    };

    std::vector<AssetEntry> s_assets;
    bool s_initialized = false;
    char s_search[128] = "";
    std::string s_pipelineStatus;

    enum class AssetTab
    {
        All,
        Scenes,
        Prefabs,
        Models,
        Textures
    };

    AssetTab s_activeTab = AssetTab::All;

    std::string toLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::string pathToUtf8(const std::filesystem::path& path)
    {
        const std::u8string u8 = path.generic_u8string();
        return std::string(u8.begin(), u8.end());
    }

    AssetKind detectKind(const std::filesystem::path& path)
    {
        const std::string ext = toLower(path.extension().string());
        if (ext == ".prefab") return AssetKind::Prefab;
        if (ext == ".scn") return AssetKind::Scene;
        if (ext == ".fbx") return AssetKind::Fbx;

        if (ext == ".dds" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".hdr")
        {
            return AssetKind::Texture;
        }

        return AssetKind::Other;
    }

    const char* kindToText(AssetKind kind)
    {
        switch (kind)
        {
        case AssetKind::Prefab: return "Prefab";
        case AssetKind::Scene: return "Scene";
        case AssetKind::Fbx: return "Model";
        case AssetKind::Texture: return "Texture";
        default: return "Other";
        }
    }

    bool isManagedAsset(AssetKind kind)
    {
        return kind == AssetKind::Prefab || kind == AssetKind::Scene || kind == AssetKind::Fbx || kind == AssetKind::Texture;
    }

    std::filesystem::path getAssetRoot()
    {
        return std::filesystem::current_path() / "Data";
    }

    void rebuildAssetList()
    {
        s_assets.clear();
        EditorAssetMeta::AssetMetaManager::Instance().clearCache();

        const std::filesystem::path root = getAssetRoot();
        std::error_code ec;
        if (!std::filesystem::exists(root, ec))
        {
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (ec)
            {
                continue;
            }

            if (!entry.is_regular_file(ec))
            {
                continue;
            }

            const AssetKind kind = detectKind(entry.path());
            if (!isManagedAsset(kind))
            {
                continue;
            }

            std::error_code relEc;
            const std::filesystem::path rel = std::filesystem::relative(entry.path(), std::filesystem::current_path(), relEc);

            AssetEntry asset;
            asset.absolutePath = entry.path();
            asset.relativePath = relEc ? entry.path().filename() : rel;
            asset.kind = kind;

            const auto& meta = EditorAssetMeta::AssetMetaManager::Instance().getOrCreate(asset.absolutePath);
            asset.guid = meta.guid;
            asset.importer = meta.importer;
            asset.thumbnailMode = meta.thumbnailMode;
            s_assets.push_back(std::move(asset));
        }

        std::sort(s_assets.begin(), s_assets.end(),
            [](const AssetEntry& a, const AssetEntry& b)
            {
                return a.relativePath.native() < b.relativePath.native();
            });
    }

    bool passesSearch(const AssetEntry& asset)
    {
        if (s_search[0] == '\0')
        {
            return true;
        }

        const std::string needle = toLower(std::string(s_search));
        const std::string haystack = toLower(pathToUtf8(asset.relativePath));
        return haystack.find(needle) != std::string::npos;
    }

    bool passesTabFilter(const AssetEntry& asset)
    {
        switch (s_activeTab)
        {
        case AssetTab::Scenes: return asset.kind == AssetKind::Scene;
        case AssetTab::Prefabs: return asset.kind == AssetKind::Prefab;
        case AssetTab::Models: return asset.kind == AssetKind::Fbx;
        case AssetTab::Textures: return asset.kind == AssetKind::Texture;
        case AssetTab::All:
        default:
            return true;
        }
    }

    bool isVisibleAsset(const AssetEntry& asset)
    {
        return passesSearch(asset) && passesTabFilter(asset);
    }

    void handleAssetOpen(const AssetEntry& asset)
    {
        EditorAssetDragDrop::applyAssetToScene(asset.absolutePath, nullptr);
    }

    void drawPreviewCell(const AssetEntry& asset)
    {
        ImTextureID texID = EditorAssetThumbnail::AssetThumbnailManager::Instance().getThumbnail(
            asset.absolutePath,
            asset.importer,
            asset.thumbnailMode);

        if (texID != ImTextureID_Invalid)
        {
            ImGui::Image(texID, ImVec2(48.0f, 48.0f));
            const std::string label = pathToUtf8(asset.relativePath);
            EditorAssetDragDrop::beginAssetDragSource(asset.absolutePath, label.c_str());
            return;
        }

        ImGui::Button(kindToText(asset.kind), ImVec2(48.0f, 48.0f));
        const std::string label = pathToUtf8(asset.relativePath);
        EditorAssetDragDrop::beginAssetDragSource(asset.absolutePath, label.c_str());
    }
}

void drawAssetBrowserWindow()
{
    if (!s_initialized)
    {
        rebuildAssetList();
        s_initialized = true;
    }

    ImGui::Begin("Project");

    if (ImGui::Button("Refresh"))
    {
        EditorAssetThumbnail::AssetThumbnailManager::Instance().clear();
        rebuildAssetList();
        s_pipelineStatus = "Refreshed asset list.";
    }

    ImGui::SameLine();
    if (ImGui::Button("Reimport All"))
    {
        const bool ok = EditorAssetMeta::AssetMetaManager::Instance().refreshAllAssets(getAssetRoot());
        EditorAssetThumbnail::AssetThumbnailManager::Instance().clear();
        rebuildAssetList();
        s_pipelineStatus = ok ? "Reimport all completed." : "Reimport all completed with warnings.";
    }

    ImGui::SameLine();
    if (ImGui::Button("Cook"))
    {
        EditorAssetMeta::CookReport report{};
        const bool ok = EditorAssetMeta::AssetMetaManager::Instance().cookAssets(
            getAssetRoot(),
            std::filesystem::current_path() / "CookedData",
            &report);

        if (ok)
        {
            s_pipelineStatus = "Cook complete: assets=" + std::to_string(report.cookedAssetCount)
                + " copied=" + std::to_string(report.copiedFileCount)
                + " manifest=" + pathToUtf8(report.manifestPath);
        }
        else
        {
            s_pipelineStatus = "Cook failed. Check logs.";
        }
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##AssetSearch", "Search assets...", s_search, IM_ARRAYSIZE(s_search));

    if (!s_pipelineStatus.empty())
    {
        ImGui::TextWrapped("%s", s_pipelineStatus.c_str());
    }

    const auto& asyncLoader = EditorAsyncAsset::AsyncAssetLoader::Instance();
    if (asyncLoader.isBusy() || !asyncLoader.getLastStatus().empty())
    {
        ImGui::TextWrapped("Async Load: %s (pending=%zu)",
            asyncLoader.getLastStatus().c_str(),
            asyncLoader.pendingTaskCount());
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("##AssetTabs"))
    {
        if (ImGui::BeginTabItem("All")) { s_activeTab = AssetTab::All; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Scenes")) { s_activeTab = AssetTab::Scenes; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Prefabs")) { s_activeTab = AssetTab::Prefabs; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Models")) { s_activeTab = AssetTab::Models; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Textures")) { s_activeTab = AssetTab::Textures; ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::Separator();

    const std::filesystem::path root = getAssetRoot();
    const std::string rootText = pathToUtf8(root);
    ImGui::Text("Root: %s", rootText.c_str());

    if (ImGui::BeginTable("AssetTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("GUID", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < s_assets.size(); ++i)
        {
            const AssetEntry& asset = s_assets[i];
            if (!isVisibleAsset(asset))
            {
                continue;
            }

            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            drawPreviewCell(asset);

            ImGui::TableSetColumnIndex(1);
            const std::string name = pathToUtf8(asset.absolutePath.filename());
            const bool clicked = ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
            const std::string dragLabel = pathToUtf8(asset.relativePath);
            EditorAssetDragDrop::beginAssetDragSource(asset.absolutePath, dragLabel.c_str());
            if (clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                handleAssetOpen(asset);
            }

            if (ImGui::BeginPopupContextItem("##AssetContext"))
            {
                if (ImGui::MenuItem("Open"))
                {
                    handleAssetOpen(asset);
                }

                if (ImGui::MenuItem("Reimport / Refresh Meta"))
                {
                    EditorAssetMeta::ReimportReport report{};
                    EditorAssetMeta::AssetMetaManager::Instance().refreshAsset(asset.absolutePath, &report);
                    EditorAssetThumbnail::AssetThumbnailManager::Instance().clear();
                    rebuildAssetList();

                    s_pipelineStatus = "Reimport: " + pathToUtf8(asset.relativePath)
                        + " srcChanged=" + (report.sourceChanged ? "1" : "0")
                        + " depChanged=" + (report.dependencyChanged ? "1" : "0")
                        + " importerChanged=" + (report.importerVersionChanged ? "1" : "0")
                        + " reimported=" + (report.reimported ? "1" : "0");
                }

                if (ImGui::MenuItem("Show Dependencies"))
                {
                    const auto deps = EditorAssetMeta::AssetMetaManager::Instance().getDependencies(asset.absolutePath);
                    const auto users = EditorAssetMeta::AssetMetaManager::Instance().getDependents(asset.absolutePath);

                    std::string message = "deps=" + std::to_string(deps.size()) + " users=" + std::to_string(users.size());
                    for (const auto& dep : deps)
                    {
                        message += " | dep:" + pathToUtf8(dep);
                    }
                    for (const auto& user : users)
                    {
                        message += " | usedBy:" + pathToUtf8(user);
                    }
                    s_pipelineStatus = message;
                }

                if (ImGui::MenuItem("Copy GUID"))
                {
                    ImGui::SetClipboardText(asset.guid.c_str());
                }

                ImGui::EndPopup();
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(kindToText(asset.kind));

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(asset.guid.c_str());

            ImGui::TableSetColumnIndex(4);
            const std::string relativePathText = pathToUtf8(asset.relativePath);
            ImGui::TextUnformatted(relativePathText.c_str());

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}