#include "pch.h"
#include "SceneFlatBuffer.h"

#include "Generated/Scene_generated.h"
#include "SerializationCommon.h"

namespace
{
    constexpr uint32_t kSceneVersion = 1;

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

}

namespace SceneFlatBuffer
{
    bool save(const std::filesystem::path& filePath, SceneId sceneId)
    {
        std::vector<GameObject*> objects;
        for (GameObject* object : GameObjectRegistry::Instance().getAll())
        {
            if (object && !object->isDestroyed())
            {
                objects.push_back(object);
            }
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

            serializedObjects.push_back(scene::CreateSerializedGameObjectDirect(
                builder,
                object->getName().c_str(),
                object->isEnabled(),
                parentIndex,
                &tags,
                &components,
                object->getPrefabAssetPath().empty() ? nullptr : object->getPrefabAssetPath().c_str()));
        }

        const auto sceneRoot = scene::CreateSerializedSceneDirect(
            builder,
            kSceneVersion,
            static_cast<int32_t>(sceneId),
            &serializedObjects);

        scene::FinishSerializedSceneBuffer(builder, sceneRoot);

        if (!writeFileBytes(filePath, builder.GetBufferPointer(), builder.GetSize()))
        {
            LOG_ERROR("[SceneFlatBuffer] Failed to write file: %s", filePath.string().c_str());
            return false;
        }

        if (!SerializationCommon::saveAnimatorBindings(filePath, objects, "SceneFlatBuffer"))
        {
            LOG_WARN("[SceneFlatBuffer] Failed to save animator bindings: %s", filePath.string().c_str());
        }

        LOG_INFO("[SceneFlatBuffer] Saved scene: %s", filePath.string().c_str());
        return true;
    }

    bool load(const std::filesystem::path& filePath, SceneId currentSceneId, SceneId* outSceneId)
    {
        const std::vector<uint8_t> bytes = readFileBytes(filePath);
        if (bytes.empty())
        {
            LOG_ERROR("[SceneFlatBuffer] Failed to read file: %s", filePath.string().c_str());
            return false;
        }

        if (!scene::SerializedSceneBufferHasIdentifier(bytes.data()))
        {
            LOG_ERROR("[SceneFlatBuffer] Invalid file identifier: %s", filePath.string().c_str());
            return false;
        }

        flatbuffers::Verifier verifier(bytes.data(), bytes.size());
        const scene::SerializedScene* root = scene::GetSerializedScene(bytes.data());
        if (!root || !root->Verify(verifier))
        {
            LOG_ERROR("[SceneFlatBuffer] Invalid flatbuffer scene: %s", filePath.string().c_str());
            return false;
        }

        const SceneId loadedSceneId = static_cast<SceneId>(root->scene_id());
        if (outSceneId)
        {
            *outSceneId = loadedSceneId;
        }

        if (loadedSceneId != currentSceneId)
        {
            LOG_WARN("[SceneFlatBuffer] SceneId mismatch. file=%d current=%d", static_cast<int>(loadedSceneId), static_cast<int>(currentSceneId));
        }

        GameObjectRegistry::Instance().shutdown();

        const auto* serializedObjects = root->objects();
        if (!serializedObjects)
        {
            return true;
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
                    SerializationCommon::deserializeComponent(object, serializedComponent, "SceneFlatBuffer");
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

        SerializationCommon::loadAnimatorBindings(filePath, newObjects, "SceneFlatBuffer");

        LOG_INFO("[SceneFlatBuffer] Loaded scene: %s", filePath.string().c_str());
        return true;
    }
}
