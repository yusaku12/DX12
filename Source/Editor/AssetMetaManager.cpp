#include "pch.h"
#include "AssetMetaManager.h"
#include "Generated/Prefab_generated.h"
#include "Generated/Scene_generated.h"
#include "Model/FbxLoad.h"

#pragma comment(lib, "Ole32.lib")

namespace
{
    using DependencySet = std::unordered_set<std::filesystem::path>;

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

    std::filesystem::path toCookPath(const std::filesystem::path& sourceRoot, const std::filesystem::path& cookRoot, const std::filesystem::path& sourcePath)
    {
        std::error_code ec;
        const std::filesystem::path relative = std::filesystem::relative(sourcePath, sourceRoot, ec);
        if (!ec)
        {
            return cookRoot / relative;
        }

        return cookRoot / sourcePath.filename();
    }

    std::string toUtf8Path(const std::filesystem::path& path)
    {
        const std::u8string u8 = path.generic_u8string();
        return std::string(u8.begin(), u8.end());
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

    std::filesystem::path toRelativeOrNormalized(const std::filesystem::path& path)
    {
        std::error_code ec;
        const std::filesystem::path relative = std::filesystem::relative(path, std::filesystem::current_path(), ec);
        if (!ec)
        {
            return relative.lexically_normal();
        }

        return path.lexically_normal();
    }

    std::filesystem::path toRelativeNormalizedStringPath(const std::filesystem::path& path)
    {
        return toRelativeOrNormalized(path).lexically_normal();
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

    bool isManagedAsset(const std::filesystem::path& assetPath)
    {
        const std::string ext = toLower(assetPath.extension().string());
        return ext == ".prefab"
            || ext == ".scn"
            || ext == ".fbx"
            || ext == ".dds"
            || ext == ".png"
            || ext == ".jpg"
            || ext == ".jpeg"
            || ext == ".bmp"
            || ext == ".tga"
            || ext == ".hdr";
    }

    uint32_t importerVersionFor(std::string_view importer)
    {
        if (importer == "FbxImporter") return 2;
        if (importer == "PrefabImporter") return 1;
        if (importer == "SceneImporter") return 1;
        return 1;
    }

    uint64_t fnv1a64(const uint8_t* data, size_t size)
    {
        uint64_t hash = 1469598103934665603ull;
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= static_cast<uint64_t>(data[i]);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    uint64_t combineHash64(uint64_t a, uint64_t b)
    {
        const uint64_t kMul = 0x9e3779b97f4a7c15ull;
        uint64_t h = a;
        h ^= b + kMul + (h << 6) + (h >> 2);
        return h;
    }

    std::vector<uint8_t> readBinary(const std::filesystem::path& filePath)
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file)
        {
            return {};
        }

        const std::streamsize size = file.tellg();
        if (size <= 0)
        {
            return {};
        }

        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
        {
            return {};
        }

        return bytes;
    }

    uint64_t fileHash(const std::filesystem::path& filePath)
    {
        const std::vector<uint8_t> bytes = readBinary(filePath);
        if (bytes.empty())
        {
            return 0;
        }

        return fnv1a64(bytes.data(), bytes.size());
    }

    void addDependencyPath(DependencySet& out, const std::filesystem::path& ownerAsset, std::string_view rawPath)
    {
        if (rawPath.empty())
        {
            return;
        }

        const std::filesystem::path asPath = fromUtf8Path(rawPath);

        std::filesystem::path resolved;
        if (asPath.is_absolute())
        {
            resolved = normalizePathKey(asPath);
        }
        else
        {
            const std::filesystem::path candidateA = normalizePathKey(std::filesystem::current_path() / asPath);
            const std::filesystem::path candidateB = normalizePathKey(ownerAsset.parent_path() / asPath);

            std::error_code ec;
            if (std::filesystem::exists(candidateA, ec))
            {
                resolved = candidateA;
            }
            else
            {
                ec.clear();
                if (std::filesystem::exists(candidateB, ec))
                {
                    resolved = candidateB;
                }
                else
                {
                    resolved = candidateA;
                }
            }
        }

        if (resolved.empty() || resolved == ownerAsset)
        {
            return;
        }

        out.insert(toRelativeNormalizedStringPath(resolved));
    }

    void collectSceneDependencies(const std::filesystem::path& assetPath, const std::vector<uint8_t>& bytes, DependencySet& out)
    {
        if (!scene::SerializedSceneBufferHasIdentifier(bytes.data()))
        {
            return;
        }

        flatbuffers::Verifier verifier(bytes.data(), bytes.size());
        const scene::SerializedScene* root = scene::GetSerializedScene(bytes.data());
        if (!root || !root->Verify(verifier))
        {
            return;
        }

        const auto* objects = root->objects();
        if (!objects)
        {
            return;
        }

        for (const scene::SerializedGameObject* object : *objects)
        {
            if (!object)
            {
                continue;
            }

            if (const auto* prefabPath = object->prefab_asset_path())
            {
                addDependencyPath(out, assetPath, prefabPath->string_view());
            }

            if (const auto* components = object->components())
            {
                for (const scene::SerializedComponent* component : *components)
                {
                    if (!component)
                    {
                        continue;
                    }

                    switch (component->payload_type())
                    {
                    case scene::ComponentPayload_FbxRenderComponentData:
                    {
                        const auto* payload = component->payload_as_FbxRenderComponentData();
                        if (payload && payload->model_path())
                        {
                            addDependencyPath(out, assetPath, payload->model_path()->string_view());
                        }
                        break;
                    }
                    case scene::ComponentPayload_SkyboxComponentData:
                    {
                        const auto* payload = component->payload_as_SkyboxComponentData();
                        if (payload && payload->cubemap_path())
                        {
                            addDependencyPath(out, assetPath, payload->cubemap_path()->string_view());
                        }
                        break;
                    }
                    case scene::ComponentPayload_GpuEffectComponentData:
                    {
                        const auto* payload = component->payload_as_GpuEffectComponentData();
                        if (payload && payload->texture_path())
                        {
                            addDependencyPath(out, assetPath, payload->texture_path()->string_view());
                        }
                        break;
                    }
                    case scene::ComponentPayload_CpuParticleComponentData:
                    {
                        const auto* payload = component->payload_as_CpuParticleComponentData();
                        if (payload && payload->texture_path())
                        {
                            addDependencyPath(out, assetPath, payload->texture_path()->string_view());
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }

        std::filesystem::path animBind = assetPath;
        animBind += ".animbind";
        std::error_code ec;
        if (std::filesystem::exists(animBind, ec))
        {
            out.insert(toRelativeNormalizedStringPath(normalizePathKey(animBind)));
        }
    }

    void collectPrefabDependencies(const std::filesystem::path& assetPath, const std::vector<uint8_t>& bytes, DependencySet& out)
    {
        if (!scene::SerializedPrefabBufferHasIdentifier(bytes.data()))
        {
            return;
        }

        flatbuffers::Verifier verifier(bytes.data(), bytes.size());
        const scene::SerializedPrefab* root = scene::GetSerializedPrefab(bytes.data());
        if (!root || !root->Verify(verifier))
        {
            return;
        }

        const auto* objects = root->objects();
        if (!objects)
        {
            return;
        }

        for (const scene::SerializedGameObject* object : *objects)
        {
            if (!object)
            {
                continue;
            }

            if (const auto* prefabPath = object->prefab_asset_path())
            {
                addDependencyPath(out, assetPath, prefabPath->string_view());
            }

            if (const auto* components = object->components())
            {
                for (const scene::SerializedComponent* component : *components)
                {
                    if (!component)
                    {
                        continue;
                    }

                    switch (component->payload_type())
                    {
                    case scene::ComponentPayload_FbxRenderComponentData:
                    {
                        const auto* payload = component->payload_as_FbxRenderComponentData();
                        if (payload && payload->model_path())
                        {
                            addDependencyPath(out, assetPath, payload->model_path()->string_view());
                        }
                        break;
                    }
                    case scene::ComponentPayload_SkyboxComponentData:
                    {
                        const auto* payload = component->payload_as_SkyboxComponentData();
                        if (payload && payload->cubemap_path())
                        {
                            addDependencyPath(out, assetPath, payload->cubemap_path()->string_view());
                        }
                        break;
                    }
                    case scene::ComponentPayload_GpuEffectComponentData:
                    {
                        const auto* payload = component->payload_as_GpuEffectComponentData();
                        if (payload && payload->texture_path())
                        {
                            addDependencyPath(out, assetPath, payload->texture_path()->string_view());
                        }
                        break;
                    }
                    case scene::ComponentPayload_CpuParticleComponentData:
                    {
                        const auto* payload = component->payload_as_CpuParticleComponentData();
                        if (payload && payload->texture_path())
                        {
                            addDependencyPath(out, assetPath, payload->texture_path()->string_view());
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }

        std::filesystem::path animBind = assetPath;
        animBind += ".animbind";
        std::error_code ec;
        if (std::filesystem::exists(animBind, ec))
        {
            out.insert(toRelativeNormalizedStringPath(normalizePathKey(animBind)));
        }
    }

    std::vector<std::string> collectDependencies(const std::filesystem::path& assetPath)
    {
        const std::vector<uint8_t> bytes = readBinary(assetPath);
        if (bytes.empty())
        {
            return {};
        }

        DependencySet dependencies;
        const std::string ext = toLower(assetPath.extension().string());
        if (ext == ".scn")
        {
            collectSceneDependencies(assetPath, bytes, dependencies);
        }
        else if (ext == ".prefab")
        {
            collectPrefabDependencies(assetPath, bytes, dependencies);
        }

        std::vector<std::string> out;
        out.reserve(dependencies.size());
        for (const auto& path : dependencies)
        {
            out.push_back(path.generic_string());
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    uint64_t computeDependencyHash(const std::vector<std::string>& dependencies)
    {
        uint64_t hash = 1469598103934665603ull;
        for (const std::string& dependency : dependencies)
        {
            const uint64_t depHash = fileHash(normalizePathKey(fromUtf8Path(dependency)));
            hash = combineHash64(hash, depHash);
        }
        return hash;
    }

    bool runImporter(const std::filesystem::path& assetPath, const std::string& importer)
    {
        if (importer == "FbxImporter")
        {
            FbxLoad loader;
            const std::string utf8Path = toUtf8Path(assetPath);
            return loader.load(utf8Path.c_str());
        }

        return true;
    }

    EditorAssetMeta::Meta buildDefaultMeta(const std::filesystem::path& assetPath)
    {
        EditorAssetMeta::Meta meta;
        meta.schemaVersion = EditorAssetMeta::AssetMetaManager::kCurrentMetaSchemaVersion;
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

        meta.importerVersion = importerVersionFor(meta.importer);

        return meta;
    }

    bool parseUInt64(const std::string& text, uint64_t& outValue)
    {
        if (text.empty())
        {
            return false;
        }

        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
        if (end == nullptr || *end != '\0')
        {
            return false;
        }

        outValue = static_cast<uint64_t>(parsed);
        return true;
    }

    bool parseUInt32(const std::string& text, uint32_t& outValue)
    {
        uint64_t tmp = 0;
        if (!parseUInt64(text, tmp))
        {
            return false;
        }

        outValue = static_cast<uint32_t>(tmp);
        return true;
    }

    bool loadMetaFile(const std::filesystem::path& metaPath, EditorAssetMeta::Meta& inOutMeta, bool& outMigrated)
    {
        outMigrated = false;

        std::ifstream file(metaPath);
        if (!file)
        {
            return false;
        }

        bool hasSchemaVersion = false;
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

            if (key == "schemaVersion")
            {
                uint32_t parsed = 0;
                if (parseUInt32(value, parsed))
                {
                    inOutMeta.schemaVersion = parsed;
                    hasSchemaVersion = true;
                }
            }
            else if (key == "guid" && !value.empty())
            {
                inOutMeta.guid = value;
            }
            else if (key == "importer" && !value.empty())
            {
                inOutMeta.importer = value;
            }
            else if (key == "importerVersion")
            {
                uint32_t parsed = 0;
                if (parseUInt32(value, parsed))
                {
                    inOutMeta.importerVersion = parsed;
                }
            }
            else if (key == "thumbnailMode" && !value.empty())
            {
                inOutMeta.thumbnailMode = value;
            }
            else if (key == "sourceHash")
            {
                parseUInt64(value, inOutMeta.sourceHash);
            }
            else if (key == "dependencyHash")
            {
                parseUInt64(value, inOutMeta.dependencyHash);
            }
            else if (key == "lastImportedHash")
            {
                parseUInt64(value, inOutMeta.lastImportedHash);
            }
            else if (key == "dependency")
            {
                inOutMeta.dependencies.push_back(value);
            }
        }

        if (!hasSchemaVersion)
        {
            inOutMeta.schemaVersion = 1;
        }

        if (inOutMeta.schemaVersion < EditorAssetMeta::AssetMetaManager::kCurrentMetaSchemaVersion)
        {
            inOutMeta.schemaVersion = EditorAssetMeta::AssetMetaManager::kCurrentMetaSchemaVersion;
            if (inOutMeta.importerVersion == 0)
            {
                inOutMeta.importerVersion = importerVersionFor(inOutMeta.importer);
            }
            outMigrated = true;
        }

        return true;
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

        refreshAsset(key, nullptr);

        auto refreshed = m_cache.find(key);
        if (refreshed != m_cache.end())
        {
            return refreshed->second;
        }

        Meta meta = loadOrCreateMeta(key);
        updateGraph(key, meta);
        const auto insertResult = m_cache.emplace(key, std::move(meta));
        return insertResult.first->second;
    }

    bool AssetMetaManager::refreshAsset(const std::filesystem::path& assetPath, ReimportReport* outReport)
    {
        const std::filesystem::path key = normalizePathKey(assetPath);

        Meta meta = loadOrCreateMeta(key);
        const uint64_t previousSourceHash = meta.sourceHash;
        const uint64_t previousDependencyHash = meta.dependencyHash;
        const uint32_t previousImporterVersion = meta.importerVersion;

        meta.importerVersion = importerVersionFor(meta.importer);
        meta.dependencies = collectDependencies(key);
        meta.sourceHash = fileHash(key);
        meta.dependencyHash = computeDependencyHash(meta.dependencies);

        uint64_t combinedHash = meta.sourceHash;
        combinedHash = combineHash64(combinedHash, meta.dependencyHash);
        combinedHash = combineHash64(combinedHash, static_cast<uint64_t>(meta.importerVersion));

        const bool sourceChanged = previousSourceHash != meta.sourceHash;
        const bool dependencyChanged = previousDependencyHash != meta.dependencyHash;
        const bool importerVersionChanged = previousImporterVersion != meta.importerVersion;
        const bool needsReimport = sourceChanged
            || dependencyChanged
            || importerVersionChanged
            || (meta.lastImportedHash != combinedHash);

        bool reimported = false;
        if (needsReimport)
        {
            reimported = runImporter(key, meta.importer);
            if (reimported)
            {
                meta.lastImportedHash = combinedHash;
            }
            else
            {
                const std::string keyUtf8 = toUtf8Path(key);
                LOG_WARN("[AssetMetaManager] Reimport failed: %s", keyUtf8.c_str());
            }
        }

        const std::filesystem::path metaPath = toMetaPath(key);
        saveMeta(metaPath, meta);
        m_cache[key] = meta;
        updateGraph(key, meta);

        if (outReport)
        {
            outReport->schemaMigrated = (meta.schemaVersion == kCurrentMetaSchemaVersion);
            outReport->sourceChanged = sourceChanged;
            outReport->dependencyChanged = dependencyChanged;
            outReport->importerVersionChanged = importerVersionChanged;
            outReport->reimported = reimported;
            outReport->combinedHash = combinedHash;
            outReport->dependencies.clear();
            outReport->dependencies.reserve(meta.dependencies.size());
            for (const std::string& dependency : meta.dependencies)
            {
                outReport->dependencies.push_back(fromUtf8Path(dependency));
            }
        }

        return !needsReimport || reimported;
    }

    bool AssetMetaManager::refreshAllAssets(const std::filesystem::path& rootPath)
    {
        const std::filesystem::path root = normalizePathKey(rootPath);
        std::error_code ec;
        if (!std::filesystem::exists(root, ec))
        {
            return false;
        }

        bool ok = true;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (ec)
            {
                ok = false;
                continue;
            }

            if (!entry.is_regular_file(ec) || ec)
            {
                continue;
            }

            if (!isManagedAsset(entry.path()))
            {
                continue;
            }

            if (!refreshAsset(entry.path(), nullptr))
            {
                ok = false;
            }
        }

        return ok;
    }

    bool AssetMetaManager::cookAssets(const std::filesystem::path& sourceRoot,
        const std::filesystem::path& cookRoot,
        CookReport* outReport)
    {
        const std::filesystem::path source = normalizePathKey(sourceRoot);
        const std::filesystem::path cook = normalizePathKey(cookRoot);

        std::error_code ec;
        if (!std::filesystem::exists(source, ec))
        {
            return false;
        }

        std::filesystem::create_directories(cook, ec);

        std::vector<std::filesystem::path> assets;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(source, std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (ec)
            {
                continue;
            }

            if (!entry.is_regular_file(ec) || ec)
            {
                continue;
            }

            if (!isManagedAsset(entry.path()))
            {
                continue;
            }

            assets.push_back(normalizePathKey(entry.path()));
        }

        std::sort(assets.begin(), assets.end());
        assets.erase(std::unique(assets.begin(), assets.end()), assets.end());

        size_t copiedFileCount = 0;
        for (const std::filesystem::path& assetPath : assets)
        {
            refreshAsset(assetPath, nullptr);

            const std::filesystem::path dstPath = toCookPath(source, cook, assetPath);
            std::filesystem::create_directories(dstPath.parent_path(), ec);
            std::filesystem::copy_file(assetPath, dstPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (!ec)
            {
                ++copiedFileCount;
            }

            if (toLower(assetPath.extension().string()) == ".fbx")
            {
                std::filesystem::path mdlPath = assetPath;
                mdlPath.replace_extension(".mdl");
                ec.clear();
                if (std::filesystem::exists(mdlPath, ec))
                {
                    const std::filesystem::path dstMdlPath = toCookPath(source, cook, mdlPath);
                    std::filesystem::create_directories(dstMdlPath.parent_path(), ec);
                    std::filesystem::copy_file(mdlPath, dstMdlPath, std::filesystem::copy_options::overwrite_existing, ec);
                    if (!ec)
                    {
                        ++copiedFileCount;
                    }
                }
            }
        }

        const std::filesystem::path manifestPath = cook / "AssetManifest.txt";
        std::ofstream manifest(manifestPath, std::ios::trunc);
        if (!manifest)
        {
            return false;
        }

        manifest << "manifestVersion=" << kCurrentManifestVersion << "\n";
        manifest << "assetCount=" << assets.size() << "\n";

        for (const std::filesystem::path& assetPath : assets)
        {
            const Meta& meta = getOrCreate(assetPath);
            const std::filesystem::path cookedPath = toCookPath(source, cook, assetPath);

            manifest << "[asset]\n";
            manifest << "guid=" << meta.guid << "\n";
            manifest << "sourcePath=" << toRelativeOrNormalized(assetPath).generic_string() << "\n";
            manifest << "cookedPath=" << toRelativeOrNormalized(cookedPath).generic_string() << "\n";
            manifest << "schemaVersion=" << meta.schemaVersion << "\n";
            manifest << "importer=" << meta.importer << "\n";
            manifest << "importerVersion=" << meta.importerVersion << "\n";
            manifest << "sourceHash=" << meta.sourceHash << "\n";
            manifest << "dependencyHash=" << meta.dependencyHash << "\n";
            for (const std::string& dependency : meta.dependencies)
            {
                manifest << "dependency=" << dependency << "\n";
            }
            manifest << "[/asset]\n";
        }

        if (!manifest.good())
        {
            return false;
        }

        if (outReport)
        {
            outReport->cookedAssetCount = assets.size();
            outReport->copiedFileCount = copiedFileCount;
            outReport->manifestPath = manifestPath;
        }

        LOG_INFO("[AssetMetaManager] Cook completed. assets=%zu copied=%zu manifest=%s",
            assets.size(),
            copiedFileCount,
            toUtf8Path(manifestPath).c_str());

        return true;
    }

    std::vector<std::filesystem::path> AssetMetaManager::getDependencies(const std::filesystem::path& assetPath) const
    {
        const std::filesystem::path key = normalizePathKey(assetPath);
        std::vector<std::filesystem::path> out;

        const auto it = m_dependencies.find(key);
        if (it == m_dependencies.end())
        {
            return out;
        }

        out.reserve(it->second.size());
        for (const auto& dependency : it->second)
        {
            out.push_back(dependency);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    std::vector<std::filesystem::path> AssetMetaManager::getDependents(const std::filesystem::path& assetPath) const
    {
        const std::filesystem::path key = normalizePathKey(assetPath);
        std::vector<std::filesystem::path> out;

        const auto it = m_dependents.find(key);
        if (it == m_dependents.end())
        {
            return out;
        }

        out.reserve(it->second.size());
        for (const auto& dependent : it->second)
        {
            out.push_back(dependent);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    void AssetMetaManager::clearCache()
    {
        m_cache.clear();
        m_dependencies.clear();
        m_dependents.clear();
    }

    Meta AssetMetaManager::loadOrCreateMeta(const std::filesystem::path& assetPath)
    {
        if (assetPath.empty())
        {
            LOG_WARN("[AssetMetaManager] Empty asset path in loadOrCreateMeta");
            return buildDefaultMeta(assetPath);
        }

        const std::filesystem::path metaPath = toMetaPath(assetPath);
        Meta meta = buildDefaultMeta(assetPath);

        bool migrated = false;

        std::error_code existsEc;
        if (std::filesystem::exists(metaPath, existsEc))
        {
            meta.dependencies.clear();
            loadMetaFile(metaPath, meta, migrated);
            if (meta.importerVersion == 0)
            {
                meta.importerVersion = importerVersionFor(meta.importer);
            }
        }

        if (migrated)
        {
            const std::string metaPathUtf8 = toUtf8Path(metaPath);
            LOG_INFO("[AssetMetaManager] Migrated meta schema: %s", metaPathUtf8.c_str());
        }

        saveMeta(metaPath, meta);
        return meta;
    }

    bool AssetMetaManager::saveMeta(const std::filesystem::path& metaPath, const Meta& meta) const
    {
        std::ofstream out(metaPath, std::ios::trunc);
        if (!out)
        {
            const std::string metaPathUtf8 = toUtf8Path(metaPath);
            LOG_WARN("[AssetMetaManager] Failed to open meta file: %s", metaPathUtf8.c_str());
            return false;
        }

        out << "schemaVersion=" << meta.schemaVersion << "\n";
        out << "guid=" << meta.guid << "\n";
        out << "importer=" << meta.importer << "\n";
        out << "importerVersion=" << meta.importerVersion << "\n";
        out << "thumbnailMode=" << meta.thumbnailMode << "\n";
        out << "sourceHash=" << meta.sourceHash << "\n";
        out << "dependencyHash=" << meta.dependencyHash << "\n";
        out << "lastImportedHash=" << meta.lastImportedHash << "\n";
        for (const std::string& dependency : meta.dependencies)
        {
            out << "dependency=" << dependency << "\n";
        }
        return out.good();
    }

    void AssetMetaManager::updateGraph(const std::filesystem::path& assetPath, const Meta& meta)
    {
        const std::filesystem::path key = normalizePathKey(assetPath);

        auto oldIt = m_dependencies.find(key);
        if (oldIt != m_dependencies.end())
        {
            for (const auto& oldDependency : oldIt->second)
            {
                auto depIt = m_dependents.find(oldDependency);
                if (depIt == m_dependents.end())
                {
                    continue;
                }

                depIt->second.erase(key);
                if (depIt->second.empty())
                {
                    m_dependents.erase(depIt);
                }
            }
        }

        auto& newDependencies = m_dependencies[key];
        newDependencies.clear();

        for (const std::string& dependency : meta.dependencies)
        {
            const std::filesystem::path dependencyKey = normalizePathKey(fromUtf8Path(dependency));
            if (dependencyKey == key)
            {
                continue;
            }

            newDependencies.insert(dependencyKey);
            m_dependents[dependencyKey].insert(key);
        }
    }
}