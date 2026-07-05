#include "pch.h"
#include "PrefabFlatBuffer.h"

#include "Generated/Prefab_generated.h"
#include "Generated/Scene_generated.h"
#include "SerializationCommon.h"
#include "SaveDataSchema.h"

namespace
{
    void collectHierarchy(GameObject* root, std::vector<GameObject*>& out);

    std::string normalizeAssetPath(const std::filesystem::path& filePath)
    {
        std::error_code ec;
        const std::filesystem::path relative = std::filesystem::relative(filePath, std::filesystem::current_path(), ec);
        if (!ec && !relative.empty())
        {
            return relative.lexically_normal().generic_string();
        }

        return filePath.lexically_normal().generic_string();
    }

    std::vector<uint8_t> readFileBytes(const std::filesystem::path& filePath)
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

    bool writeFileBytes(const std::filesystem::path& filePath, const uint8_t* data, size_t size)
    {
        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return false;
        }

        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        return out.good();
    }

    std::filesystem::path toVariantMetaPath(const std::filesystem::path& filePath)
    {
        std::filesystem::path path = filePath;
        path += ".variant";
        return path;
    }

    bool writeVariantMeta(const std::filesystem::path& variantPath, const std::filesystem::path& basePrefabPath)
    {
        const std::filesystem::path metaPath = toVariantMetaPath(variantPath);
        std::ofstream out(metaPath, std::ios::trunc);
        if (!out)
        {
            return false;
        }

        out << "basePrefab=" << normalizeAssetPath(basePrefabPath) << "\n";
        return out.good();
    }

    bool readVariantMeta(const std::filesystem::path& prefabPath, std::filesystem::path& outBasePrefabPath)
    {
        const std::filesystem::path metaPath = toVariantMetaPath(prefabPath);
        std::ifstream in(metaPath);
        if (!in)
        {
            return false;
        }

        std::string line;
        while (std::getline(in, line))
        {
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            const std::string key = line.substr(0, eq);
            if (key != "basePrefab")
            {
                continue;
            }

            const std::string value = line.substr(eq + 1);
            if (!value.empty())
            {
                outBasePrefabPath = std::filesystem::path(value);
                return true;
            }
        }

        return false;
    }

    std::string signatureFromSerializedComponent(const scene::SerializedComponent* component)
    {
        if (!component || !component->type_name())
        {
            return {};
        }

        std::string signature;
        signature.reserve(128);
        signature += component->type_name()->str();
        signature += "|en=";
        signature += component->enabled() ? "1" : "0";

        switch (component->payload_type())
        {
        case scene::ComponentPayload_TransformComponentData:
        {
            const auto* payload = component->payload_as_TransformComponentData();
            if (payload && payload->position() && payload->rotation() && payload->scale())
            {
                signature += std::format("|p={:.4f},{:.4f},{:.4f}|r={:.4f},{:.4f},{:.4f},{:.4f}|s={:.4f},{:.4f},{:.4f}",
                    payload->position()->x(), payload->position()->y(), payload->position()->z(),
                    payload->rotation()->x(), payload->rotation()->y(), payload->rotation()->z(), payload->rotation()->w(),
                    payload->scale()->x(), payload->scale()->y(), payload->scale()->z());
            }
            break;
        }
        case scene::ComponentPayload_FbxRenderComponentData:
        {
            const auto* payload = component->payload_as_FbxRenderComponentData();
            signature += "|model=";
            signature += (payload && payload->model_path()) ? payload->model_path()->str() : "";
            break;
        }
        case scene::ComponentPayload_GpuEffectComponentData:
        {
            const auto* payload = component->payload_as_GpuEffectComponentData();
            signature += "|tex=";
            signature += (payload && payload->texture_path()) ? payload->texture_path()->str() : "";
            signature += "|max=";
            signature += payload ? std::to_string(payload->max_particles()) : "0";
            break;
        }
        case scene::ComponentPayload_CpuParticleComponentData:
        {
            const auto* payload = component->payload_as_CpuParticleComponentData();
            signature += "|tex=";
            signature += (payload && payload->texture_path()) ? payload->texture_path()->str() : "";
            signature += "|max=";
            signature += payload ? std::to_string(payload->max_particles()) : "0";
            signature += "|etype=";
            signature += payload ? std::to_string(payload->emitter_type()) : "0";
            break;
        }
        case scene::ComponentPayload_SkyboxComponentData:
        {
            const auto* payload = component->payload_as_SkyboxComponentData();
            signature += "|cube=";
            signature += (payload && payload->cubemap_path()) ? payload->cubemap_path()->str() : "";
            signature += "|exp=";
            signature += payload ? std::to_string(payload->exposure()) : "0";
            break;
        }
        case scene::ComponentPayload_AnimationComponentData:
        {
            const auto* payload = component->payload_as_AnimationComponentData();
            signature += "|sm=";
            signature += (payload && payload->state_machine_enabled()) ? "1" : "0";
            signature += "|spd=";
            signature += payload ? std::to_string(payload->speed()) : "0";
            break;
        }
        default:
            signature += "|payload=" + std::to_string(static_cast<int>(component->payload_type()));
            break;
        }

        return signature;
    }

    std::vector<std::string> buildComponentSignaturesFromLive(GameObject* object)
    {
        std::vector<std::string> signatures;
        if (!object)
        {
            return signatures;
        }

        for (const auto& component : object->getComponents())
        {
            if (!component)
            {
                continue;
            }

            flatbuffers::FlatBufferBuilder builder(2048);
            const auto serialized = SerializationCommon::serializeComponent(builder, component.get());
            if (serialized.o == 0)
            {
                continue;
            }

            const scene::SerializedComponent* view = flatbuffers::GetTemporaryPointer<scene::SerializedComponent>(builder, serialized);
            signatures.push_back(signatureFromSerializedComponent(view));
        }

        std::sort(signatures.begin(), signatures.end());
        return signatures;
    }

    std::vector<std::string> buildComponentSignaturesFromSerialized(const scene::SerializedGameObject* object)
    {
        std::vector<std::string> signatures;
        if (!object)
        {
            return signatures;
        }

        const auto* components = object->components();
        if (!components)
        {
            return signatures;
        }

        signatures.reserve(components->size());
        for (const auto* component : *components)
        {
            signatures.push_back(signatureFromSerializedComponent(component));
        }
        std::sort(signatures.begin(), signatures.end());
        return signatures;
    }

    struct ObjectSnapshot
    {
        std::string name;
        bool enabled = true;
        int32_t parentIndex = -1;
        std::vector<int32_t> tags;
        std::vector<std::string> componentSignatures;
    };

    std::vector<ObjectSnapshot> buildLiveSnapshots(GameObject* root)
    {
        std::vector<GameObject*> objects;
        collectHierarchy(root, objects);

        std::unordered_map<GameObject*, int32_t> indices;
        for (int32_t i = 0; i < static_cast<int32_t>(objects.size()); ++i)
        {
            indices.emplace(objects[static_cast<size_t>(i)], i);
        }

        std::vector<ObjectSnapshot> out;
        out.reserve(objects.size());

        for (GameObject* object : objects)
        {
            ObjectSnapshot snap;
            snap.name = object->getName();
            snap.enabled = object->isEnabled();

            if (GameObject* parent = object->getParent())
            {
                const auto it = indices.find(parent);
                snap.parentIndex = (it != indices.end()) ? it->second : -1;
            }

            for (Tag tag : object->getTags())
            {
                snap.tags.push_back(static_cast<int32_t>(tag));
            }
            std::sort(snap.tags.begin(), snap.tags.end());

            snap.componentSignatures = buildComponentSignaturesFromLive(object);
            out.push_back(std::move(snap));
        }

        return out;
    }

    bool buildSerializedSnapshots(const std::filesystem::path& prefabPath, std::vector<ObjectSnapshot>& out)
    {
        const std::vector<uint8_t> bytes = readFileBytes(prefabPath);
        if (bytes.empty())
        {
            return false;
        }

        if (!scene::SerializedPrefabBufferHasIdentifier(bytes.data()))
        {
            return false;
        }

        flatbuffers::Verifier verifier(bytes.data(), bytes.size());
        const scene::SerializedPrefab* root = scene::GetSerializedPrefab(bytes.data());
        if (!root || !root->Verify(verifier))
        {
            return false;
        }

        const SaveDataSchema::VersionCheckResult versionCheck = SaveDataSchema::checkPrefabVersion(root->version());
        if (!versionCheck.supported)
        {
            LOG_ERROR("[PrefabFlatBuffer] Unsupported prefab schema version: %u file=%s current=%u min=%u",
                root->version(),
                prefabPath.string().c_str(),
                SaveDataSchema::kPrefabCurrentVersion,
                SaveDataSchema::kPrefabMinimumSupportedVersion);
            return false;
        }

        if (versionCheck.needsResave)
        {
            LOG_WARN("[PrefabFlatBuffer] Prefab schema is old. Please resave file=%s version=%u current=%u",
                prefabPath.string().c_str(),
                root->version(),
                SaveDataSchema::kPrefabCurrentVersion);
        }

        const auto* objects = root->objects();
        if (!objects)
        {
            return true;
        }

        out.clear();
        out.reserve(objects->size());

        for (const scene::SerializedGameObject* object : *objects)
        {
            if (!object)
            {
                continue;
            }

            ObjectSnapshot snap;
            snap.name = object->name() ? object->name()->str() : std::string();
            snap.enabled = object->enabled();
            snap.parentIndex = object->parent_index();

            if (const auto* tags = object->tags())
            {
                snap.tags.reserve(tags->size());
                for (int32_t tag : *tags)
                {
                    snap.tags.push_back(tag);
                }
                std::sort(snap.tags.begin(), snap.tags.end());
            }

            snap.componentSignatures = buildComponentSignaturesFromSerialized(object);
            out.push_back(std::move(snap));
        }

        return true;
    }


    void collectHierarchy(GameObject* root, std::vector<GameObject*>& out)
    {
        if (!root || root->isDestroyed())
        {
            return;
        }

        out.push_back(root);
        for (GameObject* child : root->getChildren())
        {
            collectHierarchy(child, out);
        }
    }

}

namespace PrefabFlatBuffer
{
    GameObject* findPrefabRoot(GameObject* object)
    {
        GameObject* current = object;
        while (current)
        {
            if (current->isPrefabInstanceRoot())
            {
                return current;
            }

            current = current->getParent();
        }

        return nullptr;
    }

    bool save(const std::filesystem::path& filePath, GameObject* root)
    {
        if (!root || root->isDestroyed())
        {
            LOG_ERROR("[PrefabFlatBuffer] Invalid prefab root");
            return false;
        }

        std::vector<GameObject*> objects;
        collectHierarchy(root, objects);
        if (objects.empty())
        {
            LOG_ERROR("[PrefabFlatBuffer] Empty prefab hierarchy");
            return false;
        }

        std::unordered_map<GameObject*, int32_t> objectIndices;
        objectIndices.reserve(objects.size());
        for (int32_t index = 0; index < static_cast<int32_t>(objects.size()); ++index)
        {
            objectIndices.emplace(objects[index], index);
        }

        flatbuffers::FlatBufferBuilder builder(64 * 1024);
        std::vector<flatbuffers::Offset<scene::SerializedGameObject>> serializedObjects;
        serializedObjects.reserve(objects.size());

        const std::string assetPath = normalizeAssetPath(filePath);

        for (GameObject* object : objects)
        {
            std::vector<int32_t> tags;
            tags.reserve(object->getTags().size());
            for (Tag tag : object->getTags())
            {
                const int32_t tagValue = static_cast<int32_t>(tag);
                if (tagValue >= 0 && tagValue < static_cast<int32_t>(Tag::MAX))
                {
                    tags.push_back(tagValue);
                }
            }

            std::vector<flatbuffers::Offset<scene::SerializedComponent>> components;
            components.reserve(object->getComponents().size());
            for (const auto& componentPtr : object->getComponents())
            {
                if (!componentPtr)
                {
                    continue;
                }

                const auto component = SerializationCommon::serializeComponent(builder, componentPtr.get());
                if (component.o != 0)
                {
                    components.push_back(component);
                }
            }

            int32_t parentIndex = -1;
            if (GameObject* parent = object->getParent())
            {
                const auto found = objectIndices.find(parent);
                if (found != objectIndices.end())
                {
                    parentIndex = found->second;
                }
            }

            const std::string prefabPath = (object == root) ? assetPath : std::string();

            serializedObjects.push_back(scene::CreateSerializedGameObjectDirect(
                builder,
                object->getName().c_str(),
                object->isEnabled(),
                parentIndex,
                &tags,
                &components,
                prefabPath.empty() ? nullptr : prefabPath.c_str()));
        }

        const auto prefabRoot = scene::CreateSerializedPrefabDirect(
            builder,
            SaveDataSchema::kPrefabCurrentVersion,
            0,
            &serializedObjects);
        scene::FinishSerializedPrefabBuffer(builder, prefabRoot);

        if (!writeFileBytes(filePath, builder.GetBufferPointer(), builder.GetSize()))
        {
            LOG_ERROR("[PrefabFlatBuffer] Failed to write file: %s", filePath.string().c_str());
            return false;
        }

        if (!SerializationCommon::saveAnimatorBindings(filePath, objects, "PrefabFlatBuffer"))
        {
            LOG_WARN("[PrefabFlatBuffer] Failed to save animator bindings: %s", filePath.string().c_str());
        }

        root->setPrefabAssetPath(assetPath);
        LOG_INFO("[PrefabFlatBuffer] Saved prefab: %s", filePath.string().c_str());
        return true;
    }

    GameObject* instantiate(const std::filesystem::path& filePath, GameObject* parent)
    {
        const std::vector<uint8_t> bytes = readFileBytes(filePath);
        if (bytes.empty())
        {
            LOG_ERROR("[PrefabFlatBuffer] Failed to read file: %s", filePath.string().c_str());
            return nullptr;
        }

        if (!scene::SerializedPrefabBufferHasIdentifier(bytes.data()))
        {
            LOG_ERROR("[PrefabFlatBuffer] Invalid file identifier: %s", filePath.string().c_str());
            return nullptr;
        }

        flatbuffers::Verifier verifier(bytes.data(), bytes.size());
        const scene::SerializedPrefab* root = scene::GetSerializedPrefab(bytes.data());
        if (!root || !root->Verify(verifier))
        {
            LOG_ERROR("[PrefabFlatBuffer] Invalid flatbuffer prefab: %s", filePath.string().c_str());
            return nullptr;
        }

        const SaveDataSchema::VersionCheckResult versionCheck = SaveDataSchema::checkPrefabVersion(root->version());
        if (!versionCheck.supported)
        {
            LOG_ERROR("[PrefabFlatBuffer] Unsupported prefab schema version: %u file=%s current=%u min=%u",
                root->version(),
                filePath.string().c_str(),
                SaveDataSchema::kPrefabCurrentVersion,
                SaveDataSchema::kPrefabMinimumSupportedVersion);
            return nullptr;
        }

        if (versionCheck.needsResave)
        {
            LOG_WARN("[PrefabFlatBuffer] Prefab schema is old. Please resave file=%s version=%u current=%u",
                filePath.string().c_str(),
                root->version(),
                SaveDataSchema::kPrefabCurrentVersion);
        }

        const auto* serializedObjects = root->objects();
        if (!serializedObjects || serializedObjects->empty())
        {
            LOG_ERROR("[PrefabFlatBuffer] Prefab has no objects: %s", filePath.string().c_str());
            return nullptr;
        }

        std::vector<GameObject*> newObjects;
        newObjects.reserve(serializedObjects->size());

        for (const scene::SerializedGameObject* serializedObject : *serializedObjects)
        {
            if (!serializedObject || !serializedObject->name())
            {
                continue;
            }

            GameObject* object = DX_NEW(GameObject, serializedObject->name()->str());

            if (const auto* components = serializedObject->components())
            {
                for (const auto* serializedComponent : *components)
                {
                    SerializationCommon::deserializeComponent(object, serializedComponent, "PrefabFlatBuffer");
                }
            }

            if (const auto* tags = serializedObject->tags())
            {
                for (int32_t tagValue : *tags)
                {
                    if (tagValue >= 0 && tagValue < static_cast<int32_t>(Tag::MAX))
                    {
                        object->addTag(static_cast<Tag>(tagValue));
                    }
                }
            }

            if (const auto* prefabPath = serializedObject->prefab_asset_path())
            {
                object->setPrefabAssetPath(prefabPath->str());
            }

            newObjects.push_back(object);
        }

        for (size_t index = 0; index < newObjects.size(); ++index)
        {
            const auto* serializedObject = serializedObjects->Get(static_cast<flatbuffers::uoffset_t>(index));
            if (!serializedObject)
            {
                continue;
            }

            const int32_t parentIndex = serializedObject->parent_index();
            if (parentIndex >= 0 && parentIndex < static_cast<int32_t>(newObjects.size()))
            {
                newObjects[index]->setParent(newObjects[static_cast<size_t>(parentIndex)]);
            }
        }

        for (size_t index = 0; index < newObjects.size(); ++index)
        {
            const auto* serializedObject = serializedObjects->Get(static_cast<flatbuffers::uoffset_t>(index));
            if (!serializedObject)
            {
                continue;
            }

            newObjects[index]->setEnabled(serializedObject->enabled());
        }

        int32_t rootIndex = root->root_object_index();
        if (rootIndex < 0 || rootIndex >= static_cast<int32_t>(newObjects.size()))
        {
            rootIndex = 0;
        }

        GameObject* prefabRoot = newObjects[static_cast<size_t>(rootIndex)];
        if (!prefabRoot)
        {
            LOG_ERROR("[PrefabFlatBuffer] Invalid root object: %s", filePath.string().c_str());
            return nullptr;
        }

        prefabRoot->setPrefabAssetPath(normalizeAssetPath(filePath));

        if (parent)
        {
            prefabRoot->setParent(parent);
        }

        SerializationCommon::loadAnimatorBindings(filePath, newObjects, "PrefabFlatBuffer");

        LOG_INFO("[PrefabFlatBuffer] Instantiated prefab: %s", filePath.string().c_str());
        return prefabRoot;
    }

    bool apply(GameObject* instanceObject)
    {
        GameObject* prefabRoot = findPrefabRoot(instanceObject);
        if (!prefabRoot)
        {
            LOG_WARN("[PrefabFlatBuffer] Apply failed. Prefab root not found");
            return false;
        }

        const std::string& path = prefabRoot->getPrefabAssetPath();
        if (path.empty())
        {
            LOG_WARN("[PrefabFlatBuffer] Apply failed. Prefab asset path is empty");
            return false;
        }

        return save(std::filesystem::path(path), prefabRoot);
    }

    GameObject* revert(GameObject* instanceObject)
    {
        GameObject* prefabRoot = findPrefabRoot(instanceObject);
        if (!prefabRoot)
        {
            LOG_WARN("[PrefabFlatBuffer] Revert failed. Prefab root not found");
            return nullptr;
        }

        const std::string path = prefabRoot->getPrefabAssetPath();
        if (path.empty())
        {
            LOG_WARN("[PrefabFlatBuffer] Revert failed. Prefab asset path is empty");
            return nullptr;
        }

        GameObject* parent = prefabRoot->getParent();
        prefabRoot->destroy();

        return instantiate(std::filesystem::path(path), parent);
    }

    bool createVariant(const std::filesystem::path& variantPath, const std::filesystem::path& basePrefabPath, GameObject* sourceRoot)
    {
        if (!save(variantPath, sourceRoot))
        {
            return false;
        }

        if (!writeVariantMeta(variantPath, basePrefabPath))
        {
            LOG_WARN("[PrefabFlatBuffer] Failed to write variant metadata: %s", variantPath.string().c_str());
        }

        if (sourceRoot)
        {
            sourceRoot->setPrefabAssetPath(normalizeAssetPath(variantPath));
        }

        LOG_INFO("[PrefabFlatBuffer] Created prefab variant: %s (base=%s)",
            variantPath.string().c_str(),
            basePrefabPath.string().c_str());

        return true;
    }

    bool buildOverrideInfo(GameObject* instanceObject, OverrideInfo& outInfo)
    {
        outInfo = {};

        GameObject* prefabRoot = findPrefabRoot(instanceObject);
        if (!prefabRoot)
        {
            return false;
        }

        const std::string& sourcePathText = prefabRoot->getPrefabAssetPath();
        if (sourcePathText.empty())
        {
            return false;
        }

        const std::filesystem::path sourcePath(sourcePathText);
        std::filesystem::path comparePath = sourcePath;

        std::filesystem::path basePath;
        if (readVariantMeta(sourcePath, basePath))
        {
            outInfo.isVariant = true;
            outInfo.basePrefabPath = basePath;
            comparePath = basePath;
        }

        std::vector<ObjectSnapshot> baseSnapshots;
        if (!buildSerializedSnapshots(comparePath, baseSnapshots))
        {
            return false;
        }

        const std::vector<ObjectSnapshot> liveSnapshots = buildLiveSnapshots(prefabRoot);

        const size_t maxCount = std::max(liveSnapshots.size(), baseSnapshots.size());
        for (size_t i = 0; i < maxCount; ++i)
        {
            if (i >= baseSnapshots.size())
            {
                outInfo.entries.push_back({
                    liveSnapshots[i].name,
                    "Object added in instance"
                    });
                continue;
            }

            if (i >= liveSnapshots.size())
            {
                outInfo.entries.push_back({
                    baseSnapshots[i].name,
                    "Object missing from instance"
                    });
                continue;
            }

            const ObjectSnapshot& live = liveSnapshots[i];
            const ObjectSnapshot& base = baseSnapshots[i];

            if (live.name != base.name)
            {
                outInfo.entries.push_back({ live.name, "Name override" });
            }
            if (live.enabled != base.enabled)
            {
                outInfo.entries.push_back({ live.name, "Enabled override" });
            }
            if (live.parentIndex != base.parentIndex)
            {
                outInfo.entries.push_back({ live.name, "Hierarchy override" });
            }
            if (live.tags != base.tags)
            {
                outInfo.entries.push_back({ live.name, "Tag override" });
            }
            if (live.componentSignatures != base.componentSignatures)
            {
                outInfo.entries.push_back({ live.name, "Component/property override" });
            }
        }

        outInfo.valid = true;
        outInfo.compareTargetPath = comparePath;
        return true;
    }
}
