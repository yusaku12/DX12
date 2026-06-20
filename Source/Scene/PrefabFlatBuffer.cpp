#include "pch.h"
#include "PrefabFlatBuffer.h"

#include "Generated/Prefab_generated.h"
#include "Generated/Scene_generated.h"

#include "Camera/CameraComponent.h"
#include "Camera/FreeCameraComponent.h"
#include "Component/AnimationComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/FbxRenderComponent.h"
#include "Component/GpuEffectComponent.h"
#include "Component/PostEffectComponent.h"
#include "Component/RigidbodyComponent.h"
#include "Component/SkyboxComponent.h"
#include "Component/TransformComponent.h"
#include "PostEffect/BloomEffect.h"
#include "PostEffect/ColorGradingEffect.h"
#include "PostEffect/DepthOfFieldEffect.h"
#include "PostEffect/MotionBlurEffect.h"

namespace
{
    constexpr uint32_t kPrefabVersion = 1;

    scene::vec3 toFlatVec3(const Vector3& value)
    {
        return scene::vec3(value.x, value.y, value.z);
    }

    scene::vec4 toFlatVec4(const Vector4& value)
    {
        return scene::vec4(value.x, value.y, value.z, value.w);
    }

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

    const char* getPostEffectTypeName(const PostEffectBase* effect)
    {
        if (dynamic_cast<const BloomEffect*>(effect)) return "BloomEffect";
        if (dynamic_cast<const ColorGradingEffect*>(effect)) return "ColorGradingEffect";
        if (dynamic_cast<const DepthOfFieldEffect*>(effect)) return "DepthOfFieldEffect";
        if (dynamic_cast<const MotionBlurEffect*>(effect)) return "MotionBlurEffect";
        return nullptr;
    }

    PostEffectBase* addPostEffectByType(PostEffectComponent* component, std::string_view typeName)
    {
        if (typeName == "BloomEffect") return component->addEffect<BloomEffect>();
        if (typeName == "ColorGradingEffect") return component->addEffect<ColorGradingEffect>();
        if (typeName == "DepthOfFieldEffect") return component->addEffect<DepthOfFieldEffect>();
        if (typeName == "MotionBlurEffect") return component->addEffect<MotionBlurEffect>();
        return nullptr;
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

    flatbuffers::Offset<scene::SerializedComponent> serializeComponent(flatbuffers::FlatBufferBuilder& builder, Component* component)
    {
        if (auto* transform = dynamic_cast<TransformComponent*>(component))
        {
            const Vector3& position = transform->getPosition();
            const Quaternion& rotationQ = transform->getRotation();
            const Vector3& scale = transform->getScale();
            const scene::vec3 positionFb = toFlatVec3(position);
            const scene::vec4 rotationFb = toFlatVec4(Vector4(rotationQ.x, rotationQ.y, rotationQ.z, rotationQ.w));
            const scene::vec3 scaleFb = toFlatVec3(scale);
            const auto payload = scene::CreateTransformComponentData(builder, &positionFb, &rotationFb, &scaleFb);

            return scene::CreateSerializedComponentDirect(
                builder,
                "TransformComponent",
                component->isEnabled(),
                scene::ComponentPayload_TransformComponentData,
                payload.Union());
        }

        if (auto* freeCamera = dynamic_cast<FreeCameraComponent*>(component))
        {
            const auto payload = scene::CreateFreeCameraComponentData(
                builder,
                freeCamera->getYaw(),
                freeCamera->getPitch(),
                freeCamera->getMoveSpeed(),
                freeCamera->getMouseSensitivity());

            return scene::CreateSerializedComponentDirect(
                builder,
                "FreeCameraComponent",
                component->isEnabled(),
                scene::ComponentPayload_FreeCameraComponentData,
                payload.Union());
        }

        if (auto* camera = dynamic_cast<CameraComponent*>(component))
        {
            const auto payload = scene::CreateCameraComponentData(
                builder,
                camera->getFov(),
                camera->getNear(),
                camera->getFar(),
                camera->getDepth(),
                static_cast<int32_t>(camera->getRenderPath()),
                static_cast<uint32_t>(camera->getRenderPassMask()));

            return scene::CreateSerializedComponentDirect(
                builder,
                "CameraComponent",
                component->isEnabled(),
                scene::ComponentPayload_CameraComponentData,
                payload.Union());
        }

        if (auto* fbx = dynamic_cast<FbxRenderComponent*>(component))
        {
            const auto modelPath = builder.CreateString(fbx->getModelPath());
            const auto payload = scene::CreateFbxRenderComponentData(builder, modelPath);

            return scene::CreateSerializedComponentDirect(
                builder,
                "FbxRenderComponent",
                component->isEnabled(),
                scene::ComponentPayload_FbxRenderComponentData,
                payload.Union());
        }

        if (auto* animation = dynamic_cast<AnimationComponent*>(component))
        {
            const auto payload = scene::CreateAnimationComponentData(
                builder,
                animation->isStateMachineEnabled(),
                animation->getSpeed());

            return scene::CreateSerializedComponentDirect(
                builder,
                "AnimationComponent",
                component->isEnabled(),
                scene::ComponentPayload_AnimationComponentData,
                payload.Union());
        }

        if (auto* rigidbody = dynamic_cast<RigidbodyComponent*>(component))
        {
            const auto payload = scene::CreateRigidbodyComponentData(
                builder,
                static_cast<int32_t>(rigidbody->getType()),
                rigidbody->isKinematic(),
                rigidbody->getMass(),
                rigidbody->getLinearDrag(),
                rigidbody->getAngularDrag(),
                rigidbody->getUseGravity(),
                rigidbody->getFreezePositionX(),
                rigidbody->getFreezePositionY(),
                rigidbody->getFreezePositionZ(),
                rigidbody->getFreezeRotationX(),
                rigidbody->getFreezeRotationY(),
                rigidbody->getFreezeRotationZ());

            return scene::CreateSerializedComponentDirect(
                builder,
                "RigidbodyComponent",
                component->isEnabled(),
                scene::ComponentPayload_RigidbodyComponentData,
                payload.Union());
        }

        if (auto* collider = dynamic_cast<ColliderComponent*>(component))
        {
            const scene::vec3 center = toFlatVec3(collider->getCenter());
            const scene::vec3 boxHalfExtents = toFlatVec3(collider->getBoxHalfExtents());

            const auto payload = scene::CreateColliderComponentData(
                builder,
                static_cast<int32_t>(collider->getShapeType()),
                &center,
                collider->isTrigger(),
                collider->isDebugDraw(),
                &boxHalfExtents,
                collider->getSphereRadius(),
                collider->getCapsuleRadius(),
                collider->getCapsuleHalfHeight());

            return scene::CreateSerializedComponentDirect(
                builder,
                "ColliderComponent",
                component->isEnabled(),
                scene::ComponentPayload_ColliderComponentData,
                payload.Union());
        }

        if (auto* postEffect = dynamic_cast<PostEffectComponent*>(component))
        {
            std::vector<flatbuffers::Offset<scene::PostEffectEntry>> effects;
            effects.reserve(postEffect->getEffects().size());

            for (const auto& effectPtr : postEffect->getEffects())
            {
                const char* typeName = getPostEffectTypeName(effectPtr.get());
                if (!typeName)
                {
                    continue;
                }

                effects.push_back(scene::CreatePostEffectEntryDirect(
                    builder,
                    typeName,
                    effectPtr->isEnabled(),
                    effectPtr->getPriority()));
            }

            const auto effectsVector = builder.CreateVector(effects);
            const auto payload = scene::CreatePostEffectComponentData(
                builder,
                postEffect->getVolumePriority(),
                postEffect->getWeight(),
                postEffect->getBlendDistance(),
                postEffect->isGlobal(),
                effectsVector);

            return scene::CreateSerializedComponentDirect(
                builder,
                "PostEffectComponent",
                component->isEnabled(),
                scene::ComponentPayload_PostEffectComponentData,
                payload.Union());
        }

        if (auto* skybox = dynamic_cast<SkyboxComponent*>(component))
        {
            const scene::vec3 tint = toFlatVec3(skybox->getTint());
            const auto pathString = builder.CreateString(wstringToString(skybox->getCubemapPath()));
            const auto payload = scene::CreateSkyboxComponentData(
                builder,
                pathString,
                skybox->getExposure(),
                skybox->getRotationDegrees(),
                &tint);

            return scene::CreateSerializedComponentDirect(
                builder,
                "SkyboxComponent",
                component->isEnabled(),
                scene::ComponentPayload_SkyboxComponentData,
                payload.Union());
        }

        if (auto* gpuEffect = dynamic_cast<GpuEffectComponent*>(component))
        {
            const auto texturePath = builder.CreateString(wstringToString(gpuEffect->getTexturePath()));
            const auto payload = scene::CreateGpuEffectComponentData(
                builder,
                texturePath,
                gpuEffect->getMaxParticles());

            return scene::CreateSerializedComponentDirect(
                builder,
                "GpuEffectComponent",
                component->isEnabled(),
                scene::ComponentPayload_GpuEffectComponentData,
                payload.Union());
        }

        return 0;
    }

    void deserializeComponentPayload(Component* component, const scene::SerializedComponent* serialized)
    {
        if (!component || !serialized)
        {
            return;
        }

        switch (serialized->payload_type())
        {
        case scene::ComponentPayload_TransformComponentData:
        {
            auto* transform = dynamic_cast<TransformComponent*>(component);
            const auto* payload = serialized->payload_as_TransformComponentData();
            if (!transform || !payload)
            {
                return;
            }

            if (const auto* position = payload->position())
            {
                transform->setPosition(Vector3(position->x(), position->y(), position->z()));
            }

            if (const auto* rotation = payload->rotation())
            {
                transform->setRotation(Quaternion(rotation->x(), rotation->y(), rotation->z(), rotation->w()));
            }

            if (const auto* scale = payload->scale())
            {
                transform->setScale(Vector3(scale->x(), scale->y(), scale->z()));
            }
            return;
        }
        case scene::ComponentPayload_FreeCameraComponentData:
        {
            auto* freeCamera = dynamic_cast<FreeCameraComponent*>(component);
            const auto* payload = serialized->payload_as_FreeCameraComponentData();
            if (!freeCamera || !payload)
            {
                return;
            }

            freeCamera->setYaw(payload->yaw());
            freeCamera->setPitch(payload->pitch());
            freeCamera->setMoveSpeed(payload->move_speed());
            freeCamera->setMouseSensitivity(payload->mouse_sensitivity());
            return;
        }
        case scene::ComponentPayload_CameraComponentData:
        {
            auto* camera = dynamic_cast<CameraComponent*>(component);
            const auto* payload = serialized->payload_as_CameraComponentData();
            if (!camera || !payload)
            {
                return;
            }

            camera->setFov(payload->fov());
            camera->setNear(payload->near_z());
            camera->setFar(payload->far_z());
            camera->setDepth(payload->depth());
            camera->setRenderPath(static_cast<RenderPath>(payload->render_path()));
            camera->setRenderPassMask(static_cast<RenderPassFlags>(payload->render_pass_mask()));
            return;
        }
        case scene::ComponentPayload_AnimationComponentData:
        {
            auto* animation = dynamic_cast<AnimationComponent*>(component);
            const auto* payload = serialized->payload_as_AnimationComponentData();
            if (!animation || !payload)
            {
                return;
            }

            animation->setStateMachineEnabled(payload->state_machine_enabled());
            animation->setSpeed(payload->speed());
            return;
        }
        case scene::ComponentPayload_RigidbodyComponentData:
        {
            auto* rigidbody = dynamic_cast<RigidbodyComponent*>(component);
            const auto* payload = serialized->payload_as_RigidbodyComponentData();
            if (!rigidbody || !payload)
            {
                return;
            }

            rigidbody->setType(static_cast<RigidbodyType>(payload->body_type()));
            rigidbody->setKinematic(payload->is_kinematic());
            rigidbody->setMass(payload->mass());
            rigidbody->setLinearDrag(payload->linear_drag());
            rigidbody->setAngularDrag(payload->angular_drag());
            rigidbody->setUseGravity(payload->use_gravity());
            rigidbody->setFreezePositionX(payload->freeze_pos_x());
            rigidbody->setFreezePositionY(payload->freeze_pos_y());
            rigidbody->setFreezePositionZ(payload->freeze_pos_z());
            rigidbody->setFreezeRotationX(payload->freeze_rot_x());
            rigidbody->setFreezeRotationY(payload->freeze_rot_y());
            rigidbody->setFreezeRotationZ(payload->freeze_rot_z());
            return;
        }
        case scene::ComponentPayload_ColliderComponentData:
        {
            auto* collider = dynamic_cast<ColliderComponent*>(component);
            const auto* payload = serialized->payload_as_ColliderComponentData();
            if (!collider || !payload)
            {
                return;
            }

            switch (static_cast<ColliderShapeType>(payload->shape_type()))
            {
            case ColliderShapeType::Box:
                if (const auto* half = payload->box_half_extents())
                {
                    collider->setBoxShape(Vector3(half->x(), half->y(), half->z()));
                }
                else
                {
                    collider->setBoxShape();
                }
                break;
            case ColliderShapeType::Sphere:
                collider->setSphereShape(payload->sphere_radius());
                break;
            case ColliderShapeType::Capsule:
                collider->setCapsuleShape(payload->capsule_radius(), payload->capsule_half_height());
                break;
            case ColliderShapeType::Plane:
                collider->setPlaneShape();
                break;
            default:
                collider->setBoxShape();
                break;
            }

            if (const auto* center = payload->center())
            {
                collider->setCenter(Vector3(center->x(), center->y(), center->z()));
            }
            collider->setTrigger(payload->is_trigger());
            collider->setDebugDraw(payload->debug_draw());
            return;
        }
        case scene::ComponentPayload_PostEffectComponentData:
        {
            auto* postEffect = dynamic_cast<PostEffectComponent*>(component);
            const auto* payload = serialized->payload_as_PostEffectComponentData();
            if (!postEffect || !payload)
            {
                return;
            }

            postEffect->setVolumePriority(payload->volume_priority());
            postEffect->setWeight(payload->weight());
            postEffect->setBlendDistance(payload->blend_distance());
            postEffect->setGlobal(payload->is_global());

            if (const auto* effects = payload->effects())
            {
                for (const auto* effectData : *effects)
                {
                    if (!effectData || !effectData->type_name())
                    {
                        continue;
                    }

                    PostEffectBase* effect = addPostEffectByType(postEffect, effectData->type_name()->string_view());
                    if (!effect)
                    {
                        continue;
                    }

                    effect->setEnabled(effectData->enabled());
                    effect->setPriority(effectData->priority());
                }
            }
            return;
        }
        case scene::ComponentPayload_SkyboxComponentData:
        {
            auto* skybox = dynamic_cast<SkyboxComponent*>(component);
            const auto* payload = serialized->payload_as_SkyboxComponentData();
            if (!skybox || !payload)
            {
                return;
            }

            if (const auto* path = payload->cubemap_path())
            {
                const std::wstring pathW = stringToWstring(path->str());
                if (!pathW.empty())
                {
                    skybox->setCubemap(pathW);
                }
            }

            skybox->setExposure(payload->exposure());
            skybox->setRotationDegrees(payload->rotation_degrees());

            if (const auto* tint = payload->tint())
            {
                skybox->setTint(Vector3(tint->x(), tint->y(), tint->z()));
            }
            return;
        }
        case scene::ComponentPayload_GpuEffectComponentData:
        {
            auto* gpuEffect = dynamic_cast<GpuEffectComponent*>(component);
            const auto* payload = serialized->payload_as_GpuEffectComponentData();
            if (!gpuEffect || !payload)
            {
                return;
            }

            if (const auto* texturePath = payload->texture_path())
            {
                const std::wstring pathW = stringToWstring(texturePath->str());
                if (!pathW.empty())
                {
                    gpuEffect->setTexture(pathW);
                }
            }

            gpuEffect->setMaxParticles(payload->max_particles());
            return;
        }
        default:
            return;
        }
    }

    Component* deserializeComponent(GameObject* gameObject, const scene::SerializedComponent* serialized)
    {
        if (!gameObject || !serialized || !serialized->type_name())
        {
            return nullptr;
        }

        const std::string_view typeName = serialized->type_name()->string_view();
        Component* component = nullptr;

        if (typeName == "TransformComponent")
        {
            component = gameObject->addComponent<TransformComponent>();
        }
        else if (typeName == "FreeCameraComponent")
        {
            component = gameObject->addComponent<FreeCameraComponent>();
        }
        else if (typeName == "CameraComponent")
        {
            component = gameObject->addComponent<CameraComponent>();
        }
        else if (typeName == "FbxRenderComponent")
        {
            std::string modelPath;
            if (const auto* payload = serialized->payload_as_FbxRenderComponentData(); payload && payload->model_path())
            {
                modelPath = payload->model_path()->str();
            }
            if (modelPath.empty())
            {
                LOG_WARN("[PrefabFlatBuffer] Empty model path in FbxRenderComponent");
                return nullptr;
            }
            component = gameObject->addComponent<FbxRenderComponent>(modelPath);
        }
        else if (typeName == "AnimationComponent")
        {
            component = gameObject->addComponent<AnimationComponent>();
        }
        else if (typeName == "RigidbodyComponent")
        {
            component = gameObject->addComponent<RigidbodyComponent>();
        }
        else if (typeName == "ColliderComponent")
        {
            component = gameObject->addComponent<ColliderComponent>();
        }
        else if (typeName == "PostEffectComponent")
        {
            component = gameObject->addComponent<PostEffectComponent>();
        }
        else if (typeName == "SkyboxComponent")
        {
            component = gameObject->addComponent<SkyboxComponent>();
        }
        else if (typeName == "GpuEffectComponent")
        {
            component = gameObject->addComponent<GpuEffectComponent>();
        }

        if (!component)
        {
            LOG_WARN("[PrefabFlatBuffer] Unsupported component type: %s", std::string(typeName).c_str());
            return nullptr;
        }

        deserializeComponentPayload(component, serialized);
        component->setEnabled(serialized->enabled());
        return component;
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

                const auto component = serializeComponent(builder, componentPtr.get());
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
                    deserializeComponent(object, serializedComponent);
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
