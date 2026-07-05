#pragma once

#include "Generated/Scene_generated.h"

#include "GameObject/GameObject.h"

#include "Camera/CameraComponent.h"
#include "Camera/FreeCameraComponent.h"
#include "Component/AnimationComponent.h"
#include "Component/BehaviorTreeComponent.h"
#include "Component/CanvasComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/CpuParticleComponent.h"
#include "Component/FbxRenderComponent.h"
#include "Component/GpuEffectComponent.h"
#include "Component/NavAgentComponent.h"
#include "Component/PostEffectComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/RigidbodyComponent.h"
#include "Component/SkyboxComponent.h"
#include "Component/TransformComponent.h"
#include "Component/UIButtonComponent.h"
#include "Component/UIImageComponent.h"
#include "Component/UITextComponent.h"
#include "PostEffect/BloomEffect.h"
#include "PostEffect/ColorGradingEffect.h"
#include "PostEffect/DepthOfFieldEffect.h"
#include "PostEffect/MotionBlurEffect.h"

namespace SerializationCommon
{
    struct ComponentArchetype
    {
        const char* typeName;
        bool addableInEditor;
        Component* (*create)(GameObject* gameObject, const scene::SerializedComponent* serialized);
    };

    inline scene::vec3 toFlatVec3(const Vector3& value)
    {
        return scene::vec3(value.x, value.y, value.z);
    }

    inline scene::vec2 toFlatVec2(const Vector2& value)
    {
        return scene::vec2(value.x, value.y);
    }

    inline scene::vec4 toFlatVec4(const Vector4& value)
    {
        return scene::vec4(value.x, value.y, value.z, value.w);
    }

    inline const char* getPostEffectTypeName(const PostEffectBase* effect)
    {
        if (dynamic_cast<const BloomEffect*>(effect)) return "BloomEffect";
        if (dynamic_cast<const ColorGradingEffect*>(effect)) return "ColorGradingEffect";
        if (dynamic_cast<const DepthOfFieldEffect*>(effect)) return "DepthOfFieldEffect";
        if (dynamic_cast<const MotionBlurEffect*>(effect)) return "MotionBlurEffect";
        return nullptr;
    }

    inline PostEffectBase* addPostEffectByType(PostEffectComponent* component, std::string_view typeName)
    {
        if (typeName == "BloomEffect") return component->addEffect<BloomEffect>();
        if (typeName == "ColorGradingEffect") return component->addEffect<ColorGradingEffect>();
        if (typeName == "DepthOfFieldEffect") return component->addEffect<DepthOfFieldEffect>();
        if (typeName == "MotionBlurEffect") return component->addEffect<MotionBlurEffect>();
        return nullptr;
    }

    template<class T>
    Component* createDefault(GameObject* gameObject, const scene::SerializedComponent*)
    {
        return gameObject ? gameObject->addComponent<T>() : nullptr;
    }

    inline Component* createFbx(GameObject* gameObject, const scene::SerializedComponent* serialized)
    {
        if (!gameObject || !serialized)
        {
            return nullptr;
        }

        std::string modelPath;
        if (const auto* payload = serialized->payload_as_FbxRenderComponentData(); payload && payload->model_path())
        {
            modelPath = payload->model_path()->str();
        }

        if (modelPath.empty())
        {
            LOG_WARN("[SerializationCommon] Empty model path in FbxRenderComponent");
            return nullptr;
        }

        return gameObject->addComponent<FbxRenderComponent>(modelPath);
    }

    inline Component* createUIImage(GameObject* gameObject, const scene::SerializedComponent* serialized)
    {
        if (!gameObject)
        {
            return nullptr;
        }

        // RectTransformComponent がなければ追加
        if (!gameObject->getComponent<RectTransformComponent>())
        {
            RectTransformComponent* rt = gameObject->addComponent<RectTransformComponent>();
            if (rt)
            {
                rt->setSize(Vector2(100.f, 100.f));  // デフォルトサイズ
            }
        }

        UIImageComponent* component = gameObject->addComponent<UIImageComponent>();
        if (!component || !serialized)
        {
            return component;
        }

        if (const auto* payload = serialized->payload_as_UIImageComponentData(); payload)
        {
            if (payload->texture_path())
            {
                const std::string& pathStr = payload->texture_path()->str();
                std::wstring texturePath;
                texturePath.assign(pathStr.begin(), pathStr.end());
                component->setTexturePath(texturePath);
            }

            if (payload->tint_color())
            {
                const scene::vec4* color = payload->tint_color();
                component->setTintColor(Vector4(color->x(), color->y(), color->z(), color->w()));
            }

            // 古いシーンデータでは alpha フィールド未保存の可能性があるため、
            // 未指定時は既定の 1.0f を維持して不可視化を防ぐ。
            const auto* imageTable = reinterpret_cast<const flatbuffers::Table*>(payload);
            if (imageTable && imageTable->GetOptionalFieldOffset(scene::UIImageComponentData::VT_ALPHA) != 0)
            {
                component->setAlpha(payload->alpha());
            }
            else
            {
                component->setAlpha(1.0f);
            }
        }

        return component;
    }

    inline const std::vector<ComponentArchetype>& getComponentArchetypes()
    {
        static const std::vector<ComponentArchetype> archetypes =
        {
            { "TransformComponent", true, &createDefault<TransformComponent> },
            { "RectTransformComponent", true, &createDefault<RectTransformComponent> },
            { "CanvasComponent", true, &createDefault<CanvasComponent> },
            { "FreeCameraComponent", true, &createDefault<FreeCameraComponent> },
            { "CameraComponent", true, &createDefault<CameraComponent> },
            { "FbxRenderComponent", false, &createFbx },
            { "AnimationComponent", true, &createDefault<AnimationComponent> },
            { "NavAgentComponent", true, &createDefault<NavAgentComponent> },
            { "BehaviorTreeComponent", true, &createDefault<BehaviorTreeComponent> },
            { "RigidbodyComponent", true, &createDefault<RigidbodyComponent> },
            { "ColliderComponent", true, &createDefault<ColliderComponent> },
            { "PostEffectComponent", true, &createDefault<PostEffectComponent> },
            { "SkyboxComponent", true, &createDefault<SkyboxComponent> },
            { "GpuEffectComponent", true, &createDefault<GpuEffectComponent> },
            { "CpuParticleComponent", true, &createDefault<CpuParticleComponent> },
            { "UITextComponent", true, &createDefault<UITextComponent> },
            { "UIButtonComponent", true, &createDefault<UIButtonComponent> },
            { "UIImageComponent", true, &createUIImage },
        };
        return archetypes;
    }

    inline const ComponentArchetype* findArchetype(std::string_view typeName)
    {
        for (const auto& archetype : getComponentArchetypes())
        {
            if (archetype.typeName == typeName)
            {
                return &archetype;
            }
        }

        return nullptr;
    }

    inline Component* addComponentByTypeName(GameObject* gameObject, std::string_view typeName)
    {
        const ComponentArchetype* archetype = findArchetype(typeName);
        if (!archetype)
        {
            return nullptr;
        }

        return archetype->create(gameObject, nullptr);
    }

    inline bool hasComponentByTypeName(GameObject* gameObject, std::string_view typeName)
    {
        if (!gameObject)
        {
            return false;
        }

        if (typeName == "TransformComponent") return gameObject->getComponent<TransformComponent>() != nullptr;
        if (typeName == "RectTransformComponent") return gameObject->getComponent<RectTransformComponent>() != nullptr;
        if (typeName == "CanvasComponent") return gameObject->getComponent<CanvasComponent>() != nullptr;
        if (typeName == "FreeCameraComponent") return gameObject->getComponent<FreeCameraComponent>() != nullptr;
        if (typeName == "CameraComponent") return gameObject->getComponent<CameraComponent>() != nullptr;
        if (typeName == "FbxRenderComponent") return gameObject->getComponent<FbxRenderComponent>() != nullptr;
        if (typeName == "AnimationComponent") return gameObject->getComponent<AnimationComponent>() != nullptr;
        if (typeName == "NavAgentComponent") return gameObject->getComponent<NavAgentComponent>() != nullptr;
        if (typeName == "BehaviorTreeComponent") return gameObject->getComponent<BehaviorTreeComponent>() != nullptr;
        if (typeName == "RigidbodyComponent") return gameObject->getComponent<RigidbodyComponent>() != nullptr;
        if (typeName == "ColliderComponent") return gameObject->getComponent<ColliderComponent>() != nullptr;
        if (typeName == "PostEffectComponent") return gameObject->getComponent<PostEffectComponent>() != nullptr;
        if (typeName == "SkyboxComponent") return gameObject->getComponent<SkyboxComponent>() != nullptr;
        if (typeName == "GpuEffectComponent") return gameObject->getComponent<GpuEffectComponent>() != nullptr;
        if (typeName == "CpuParticleComponent") return gameObject->getComponent<CpuParticleComponent>() != nullptr;
        if (typeName == "UITextComponent") return gameObject->getComponent<UITextComponent>() != nullptr;
        if (typeName == "UIButtonComponent") return gameObject->getComponent<UIButtonComponent>() != nullptr;
        if (typeName == "UIImageComponent") return gameObject->getComponent<UIImageComponent>() != nullptr;

        return false;
    }

    inline flatbuffers::Offset<scene::SerializedComponent> serializeComponent(flatbuffers::FlatBufferBuilder& builder, Component* component)
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

        if (auto* rectTransform = dynamic_cast<RectTransformComponent*>(component))
        {
            const scene::vec2 anchor = toFlatVec2(rectTransform->getAnchor());
            const scene::vec2 position = toFlatVec2(rectTransform->getPosition());
            const scene::vec2 size = toFlatVec2(rectTransform->getSize());
            const scene::vec2 pivot = toFlatVec2(rectTransform->getPivot());
            const auto payload = scene::CreateRectTransformComponentData(builder, &anchor, &position, &size, &pivot);

            return scene::CreateSerializedComponentDirect(
                builder,
                "RectTransformComponent",
                component->isEnabled(),
                scene::ComponentPayload_RectTransformComponentData,
                payload.Union());
        }

        if (auto* canvas = dynamic_cast<CanvasComponent*>(component))
        {
            const auto payload = scene::CreateCanvasComponentData(
                builder,
                canvas->getSortOrder(),
                canvas->receivesInput());

            return scene::CreateSerializedComponentDirect(
                builder,
                "CanvasComponent",
                component->isEnabled(),
                scene::ComponentPayload_CanvasComponentData,
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

        if (auto* navAgent = dynamic_cast<NavAgentComponent*>(component))
        {
            const auto navMeshPath = builder.CreateString(navAgent->getNavMeshAssetPath());
            const auto payload = scene::CreateNavAgentComponentData(
                builder,
                navMeshPath,
                navAgent->getMoveSpeed(),
                navAgent->getAcceleration(),
                navAgent->getStoppingDistance(),
                navAgent->getRepathInterval());

            return scene::CreateSerializedComponentDirect(
                builder,
                "NavAgentComponent",
                component->isEnabled(),
                scene::ComponentPayload_NavAgentComponentData,
                payload.Union());
        }

        if (auto* behaviorTree = dynamic_cast<BehaviorTreeComponent*>(component))
        {
            const auto assetPath = builder.CreateString(behaviorTree->getAssetPath());
            const auto payload = scene::CreateBehaviorTreeComponentData(
                builder,
                assetPath,
                behaviorTree->isTreeEnabled(),
                behaviorTree->getTickInterval());

            return scene::CreateSerializedComponentDirect(
                builder,
                "BehaviorTreeComponent",
                component->isEnabled(),
                scene::ComponentPayload_BehaviorTreeComponentData,
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

        if (auto* cpuParticle = dynamic_cast<CpuParticleComponent*>(component))
        {
            const auto texturePath = builder.CreateString(wstringToString(cpuParticle->getTexturePath()));
            const auto meshSourceName = builder.CreateString(cpuParticle->getMeshSourceObjectName());
            const auto payload = scene::CreateCpuParticleComponentData(
                builder,
                texturePath,
                cpuParticle->getMaxParticles(),
                cpuParticle->getEmitterType(),
                cpuParticle->getEmitRate(),
                cpuParticle->getEmitRadius(),
                meshSourceName,
                cpuParticle->getCollisionMode(),
                cpuParticle->getSubUvRows(),
                cpuParticle->getSubUvCols(),
                cpuParticle->getSubUvFps());

            return scene::CreateSerializedComponentDirect(
                builder,
                "CpuParticleComponent",
                component->isEnabled(),
                scene::ComponentPayload_CpuParticleComponentData,
                payload.Union());
        }

        if (auto* text = dynamic_cast<UITextComponent*>(component))
        {
            const scene::vec4 color = toFlatVec4(text->getColor());
            const auto payload = scene::CreateUITextComponentDataDirect(
                builder,
                text->getText().c_str(),
                &color,
                text->getFontScale(),
                static_cast<int32_t>(text->getAlignment()));

            return scene::CreateSerializedComponentDirect(
                builder,
                "UITextComponent",
                component->isEnabled(),
                scene::ComponentPayload_UITextComponentData,
                payload.Union());
        }

        if (auto* button = dynamic_cast<UIButtonComponent*>(component))
        {
            const scene::vec4 normal = toFlatVec4(button->getNormalColor());
            const scene::vec4 hover = toFlatVec4(button->getHoverColor());
            const scene::vec4 pressed = toFlatVec4(button->getPressedColor());
            const scene::vec4 textColor = toFlatVec4(button->getTextColor());
            const auto payload = scene::CreateUIButtonComponentDataDirect(
                builder,
                button->getLabel().c_str(),
                button->getClickEventName().empty() ? nullptr : button->getClickEventName().c_str(),
                &normal,
                &hover,
                &pressed,
                &textColor,
                button->getFontScale(),
                button->getCornerRounding(),
                button->isInteractable(),
                button->blocksMouseInput());

            return scene::CreateSerializedComponentDirect(
                builder,
                "UIButtonComponent",
                component->isEnabled(),
                scene::ComponentPayload_UIButtonComponentData,
                payload.Union());
        }

        if (auto* image = dynamic_cast<UIImageComponent*>(component))
        {
            const std::wstring& texturePath = image->getTexturePath();
            // Convert wstring to string
            std::string texturePathStr;
            texturePathStr.reserve(texturePath.size());
            for (wchar_t c : texturePath)
            {
                texturePathStr.push_back(static_cast<char>(c));
            }
            const auto texturePathOffset = builder.CreateString(texturePathStr);
            const scene::vec4 tintColor = toFlatVec4(image->getTintColor());
            const auto payload = scene::CreateUIImageComponentData(
                builder,
                texturePathOffset,
                &tintColor,
                image->getAlpha());

            return scene::CreateSerializedComponentDirect(
                builder,
                "UIImageComponent",
                component->isEnabled(),
                scene::ComponentPayload_UIImageComponentData,
                payload.Union());
        }

        return 0;
    }

    inline void deserializeComponentPayload(Component* component, const scene::SerializedComponent* serialized)
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
        case scene::ComponentPayload_RectTransformComponentData:
        {
            auto* rectTransform = dynamic_cast<RectTransformComponent*>(component);
            const auto* payload = serialized->payload_as_RectTransformComponentData();
            if (!rectTransform || !payload)
            {
                return;
            }

            if (const auto* anchor = payload->anchor())
            {
                rectTransform->setAnchor(Vector2(anchor->x(), anchor->y()));
            }

            if (const auto* position = payload->position())
            {
                rectTransform->setPosition(Vector2(position->x(), position->y()));
            }

            if (const auto* size = payload->size())
            {
                rectTransform->setSize(Vector2(size->x(), size->y()));
            }

            if (const auto* pivot = payload->pivot())
            {
                rectTransform->setPivot(Vector2(pivot->x(), pivot->y()));
            }
            return;
        }
        case scene::ComponentPayload_CanvasComponentData:
        {
            auto* canvas = dynamic_cast<CanvasComponent*>(component);
            const auto* payload = serialized->payload_as_CanvasComponentData();
            if (!canvas || !payload)
            {
                return;
            }

            canvas->setSortOrder(payload->sort_order());
            canvas->setReceivesInput(payload->receives_input());
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
        case scene::ComponentPayload_NavAgentComponentData:
        {
            auto* navAgent = dynamic_cast<NavAgentComponent*>(component);
            const auto* payload = serialized->payload_as_NavAgentComponentData();
            if (!navAgent || !payload)
            {
                return;
            }

            if (const auto* path = payload->navmesh_asset_path())
            {
                navAgent->setNavMeshAssetPath(path->str());
            }
            navAgent->setMoveSpeed(payload->move_speed());
            navAgent->setAcceleration(payload->acceleration());
            navAgent->setStoppingDistance(payload->stopping_distance());
            navAgent->setRepathInterval(payload->repath_interval());
            return;
        }
        case scene::ComponentPayload_BehaviorTreeComponentData:
        {
            auto* behaviorTree = dynamic_cast<BehaviorTreeComponent*>(component);
            const auto* payload = serialized->payload_as_BehaviorTreeComponentData();
            if (!behaviorTree || !payload)
            {
                return;
            }

            if (const auto* path = payload->asset_path())
            {
                behaviorTree->setAssetPath(path->str());
            }

            behaviorTree->setTreeEnabled(payload->tree_enabled());
            behaviorTree->setTickInterval(payload->tick_interval());
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
        case scene::ComponentPayload_CpuParticleComponentData:
        {
            auto* cpuParticle = dynamic_cast<CpuParticleComponent*>(component);
            const auto* payload = serialized->payload_as_CpuParticleComponentData();
            if (!cpuParticle || !payload)
            {
                return;
            }

            if (const auto* texturePath = payload->texture_path())
            {
                const std::wstring pathW = stringToWstring(texturePath->str());
                if (!pathW.empty())
                {
                    cpuParticle->setTexture(pathW);
                }
            }

            if (const auto* meshSourceName = payload->mesh_source_object_name())
            {
                cpuParticle->setMeshSourceObjectName(meshSourceName->str());
            }

            cpuParticle->setMaxParticles(payload->max_particles());
            cpuParticle->setEmitterType(payload->emitter_type());
            cpuParticle->setEmitRate(payload->emit_rate());
            cpuParticle->setEmitRadius(payload->emit_radius());
            cpuParticle->setCollisionMode(payload->collision_mode());
            cpuParticle->setSubUvRows(payload->subuv_rows());
            cpuParticle->setSubUvCols(payload->subuv_cols());
            cpuParticle->setSubUvFps(payload->subuv_fps());
            return;
        }
        case scene::ComponentPayload_UITextComponentData:
        {
            auto* text = dynamic_cast<UITextComponent*>(component);
            const auto* payload = serialized->payload_as_UITextComponentData();
            if (!text || !payload)
            {
                return;
            }

            if (const auto* textValue = payload->text())
            {
                text->setText(textValue->str());
            }

            if (const auto* color = payload->color())
            {
                text->setColor(Vector4(color->x(), color->y(), color->z(), color->w()));
            }

            text->setFontScale(payload->font_scale());
            text->setAlignment(static_cast<UITextAlignment>(payload->alignment()));
            return;
        }
        case scene::ComponentPayload_UIButtonComponentData:
        {
            auto* button = dynamic_cast<UIButtonComponent*>(component);
            const auto* payload = serialized->payload_as_UIButtonComponentData();
            if (!button || !payload)
            {
                return;
            }

            if (const auto* label = payload->label())
            {
                button->setLabel(label->str());
            }

            if (const auto* eventName = payload->click_event_name())
            {
                button->setClickEventName(eventName->str());
            }

            if (const auto* normal = payload->normal_color())
            {
                button->setNormalColor(Vector4(normal->x(), normal->y(), normal->z(), normal->w()));
            }

            if (const auto* hover = payload->hover_color())
            {
                button->setHoverColor(Vector4(hover->x(), hover->y(), hover->z(), hover->w()));
            }

            if (const auto* pressed = payload->pressed_color())
            {
                button->setPressedColor(Vector4(pressed->x(), pressed->y(), pressed->z(), pressed->w()));
            }

            if (const auto* textColor = payload->text_color())
            {
                button->setTextColor(Vector4(textColor->x(), textColor->y(), textColor->z(), textColor->w()));
            }

            button->setFontScale(payload->font_scale());
            button->setCornerRounding(payload->corner_rounding());
            button->setInteractable(payload->interactable());
            button->setBlockMouseInput(payload->block_mouse_input());
            return;
        }
        case scene::ComponentPayload_UIImageComponentData:
        {
            auto* image = dynamic_cast<UIImageComponent*>(component);
            const auto* payload = serialized->payload_as_UIImageComponentData();
            if (!image || !payload)
            {
                return;
            }

            if (const auto* texturePath = payload->texture_path())
            {
                std::wstring wTexturePath;
                const std::string& pathStr = texturePath->str();
                wTexturePath.assign(pathStr.begin(), pathStr.end());
                image->setTexturePath(wTexturePath);
            }

            if (const auto* tintColor = payload->tint_color())
            {
                image->setTintColor(Vector4(tintColor->x(), tintColor->y(), tintColor->z(), tintColor->w()));
            }

            // 古いシーンデータでは alpha フィールド未保存の可能性があるため、
            // 未指定時は既定の 1.0f を維持して不可視化を防ぐ。
            const auto* imageTable = reinterpret_cast<const flatbuffers::Table*>(payload);
            if (imageTable && imageTable->GetOptionalFieldOffset(scene::UIImageComponentData::VT_ALPHA) != 0)
            {
                image->setAlpha(payload->alpha());
            }
            else
            {
                image->setAlpha(1.0f);
            }
            return;
        }
        default:
            return;
        }
    }

    inline Component* deserializeComponent(GameObject* gameObject, const scene::SerializedComponent* serialized, const char* logTag)
    {
        if (!gameObject || !serialized || !serialized->type_name())
        {
            return nullptr;
        }

        const std::string_view typeName = serialized->type_name()->string_view();
        const ComponentArchetype* archetype = findArchetype(typeName);
        if (!archetype)
        {
            LOG_WARN("[%s] Unsupported component type: %s", logTag, std::string(typeName).c_str());
            return nullptr;
        }

        Component* component = archetype->create(gameObject, serialized);
        if (!component)
        {
            return nullptr;
        }

        deserializeComponentPayload(component, serialized);
        component->setEnabled(serialized->enabled());
        return component;
    }

    inline std::filesystem::path getAnimatorBindingFilePath(const std::filesystem::path& ownerFilePath)
    {
        return std::filesystem::path(ownerFilePath.string() + ".animbind");
    }

    inline bool saveAnimatorBindings(const std::filesystem::path& ownerFilePath,
        const std::vector<GameObject*>& objects,
        const char* logTag)
    {
        const std::filesystem::path bindingPath = getAnimatorBindingFilePath(ownerFilePath);

        std::error_code ec;
        if (std::filesystem::exists(bindingPath, ec))
        {
            std::filesystem::remove(bindingPath, ec);
        }

        std::vector<std::pair<size_t, std::string>> bindings;
        bindings.reserve(objects.size());
        for (size_t i = 0; i < objects.size(); ++i)
        {
            GameObject* object = objects[i];
            if (!object || object->isDestroyed()) continue;

            auto* animation = object->getComponent<AnimationComponent>();
            if (!animation) continue;

            const std::string& controllerPath = animation->getControllerAssetPath();
            if (controllerPath.empty()) continue;

            if (!animation->saveControllerAsset())
            {
                LOG_WARN("[%s] Failed to save controller asset: %s", logTag, controllerPath.c_str());
            }

            bindings.emplace_back(i, controllerPath);
        }

        if (bindings.empty())
        {
            return true;
        }

        std::ofstream out(bindingPath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            LOG_ERROR("[%s] Failed to write animator binding file: %s", logTag, bindingPath.string().c_str());
            return false;
        }

        out << "ANIMBIND 1\n";
        for (const auto& [index, path] : bindings)
        {
            out << index << " " << std::quoted(path) << "\n";
        }

        return out.good();
    }

    inline void loadAnimatorBindings(const std::filesystem::path& ownerFilePath,
        const std::vector<GameObject*>& objects,
        const char* logTag)
    {
        const std::filesystem::path bindingPath = getAnimatorBindingFilePath(ownerFilePath);

        std::ifstream in(bindingPath, std::ios::binary);
        if (!in)
        {
            return;
        }

        std::string header;
        int version = 0;
        if (!(in >> header >> version) || header != "ANIMBIND" || version != 1)
        {
            LOG_WARN("[%s] Invalid animator binding file: %s", logTag, bindingPath.string().c_str());
            return;
        }

        while (in)
        {
            size_t index = 0;
            std::string path;
            if (!(in >> index >> std::quoted(path)))
            {
                break;
            }

            if (index >= objects.size())
            {
                continue;
            }

            GameObject* object = objects[index];
            if (!object || object->isDestroyed())
            {
                continue;
            }

            auto* animation = object->getComponent<AnimationComponent>();
            if (!animation)
            {
                continue;
            }

            animation->setControllerAssetPath(path);
            if (!animation->reloadControllerAsset())
            {
                LOG_WARN("[%s] Failed to load controller asset: %s", logTag, path.c_str());
            }
        }
    }
}
