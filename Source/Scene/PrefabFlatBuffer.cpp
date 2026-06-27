#include "pch.h"
#include "PrefabFlatBuffer.h"

#include "Generated/Prefab_generated.h"
#include "Generated/Scene_generated.h"
#include "SerializationCommon.h"

namespace
{
    constexpr uint32_t kPrefabVersion = 1;

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
            kPrefabVersion,
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

            GameObject* object = new GameObject(serializedObject->name()->str());

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
}
