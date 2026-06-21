#include "pch.h"
#include "AssetMetaManager.h"

#pragma comment(lib, "Ole32.lib")

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

    std::filesystem::path toMetaPath(const std::filesystem::path& assetPath)
    {
        std::filesystem::path result = assetPath;
        result += ".meta";
        return result;
    }

    std::string toLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    std::string generateGuid()
    {
        GUID guid{};
        if (CoCreateGuid(&guid) != S_OK)
        {
            return "guid-generate-failed";
        }

        wchar_t buffer[64]{};
        StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
        std::wstring ws(buffer);

        std::string out;
        out.reserve(ws.size());
        for (wchar_t c : ws)
        {
            if (c == L'{' || c == L'}')
            {
                continue;
            }

            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    }

    EditorAssetMeta::Meta buildDefaultMeta(const std::filesystem::path& assetPath)
    {
        EditorAssetMeta::Meta meta;
        meta.guid = generateGuid();

        const std::string ext = toLower(assetPath.extension().string());
        if (ext == ".prefab")
        {
            meta.importer = "PrefabImporter";
            meta.thumbnailMode = "Icon";
        }
        else if (ext == ".scn")
        {
            meta.importer = "SceneImporter";
            meta.thumbnailMode = "Icon";
        }
        else if (ext == ".fbx")
        {
            meta.importer = "FbxImporter";
            meta.thumbnailMode = "Icon";
        }
        else
        {
            meta.importer = "TextureImporter";
            // DDS はプレビュー読み込み失敗の影響を避けるため既定を Icon にする
            meta.thumbnailMode = (ext == ".dds") ? "Icon" : "Auto";
        }

        return meta;
    }

    std::unordered_map<std::string, std::string> parseMetaFile(const std::filesystem::path& metaPath)
    {
        std::unordered_map<std::string, std::string> values;

        std::ifstream file(metaPath);
        if (!file)
        {
            return values;
        }

        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty())
            {
                continue;
            }

            const size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            const std::string key = line.substr(0, eq);
            const std::string value = line.substr(eq + 1);
            values[key] = value;
        }

        return values;
    }
}

namespace EditorAssetMeta
{
    const Meta& AssetMetaManager::getOrCreate(const std::filesystem::path& assetPath)
    {
        const std::filesystem::path key = normalizePathKey(assetPath);
        auto it = m_cache.find(key);
        if (it != m_cache.end())
        {
            return it->second;
        }

        Meta meta = loadOrCreateMeta(key);
        auto insertResult = m_cache.emplace(key, std::move(meta));
        return insertResult.first->second;
    }

    void AssetMetaManager::clearCache()
    {
        m_cache.clear();
    }

    Meta AssetMetaManager::loadOrCreateMeta(const std::filesystem::path& assetPath)
    {
        const std::filesystem::path metaPath = toMetaPath(assetPath);
        Meta meta = buildDefaultMeta(assetPath);

        if (std::filesystem::exists(metaPath))
        {
            const auto values = parseMetaFile(metaPath);
            if (const auto it = values.find("guid"); it != values.end() && !it->second.empty())
            {
                meta.guid = it->second;
            }
            if (const auto it = values.find("importer"); it != values.end() && !it->second.empty())
            {
                meta.importer = it->second;
            }
            if (const auto it = values.find("thumbnailMode"); it != values.end() && !it->second.empty())
            {
                meta.thumbnailMode = it->second;
            }
        }

        saveMeta(metaPath, meta);
        return meta;
    }

    bool AssetMetaManager::saveMeta(const std::filesystem::path& metaPath, const Meta& meta) const
    {
        std::ofstream out(metaPath, std::ios::trunc);
        if (!out)
        {
            LOG_WARN("[AssetMetaManager] Failed to open meta file: %s", metaPath.string().c_str());
            return false;
        }

        out << "guid=" << meta.guid << "\n";
        out << "importer=" << meta.importer << "\n";
        out << "thumbnailMode=" << meta.thumbnailMode << "\n";
        return out.good();
    }
}