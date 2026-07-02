#include "pch.h"
#include "EngineTestRunner.h"

#include "Scene/SceneManager.h"
#include "System/TimeManager.h"

#include "Animation/AnimationStateMachine.h"
#include "Component/CanvasComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/RigidbodyComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/TransformComponent.h"
#include "Component/UIButtonComponent.h"
#include "Component/UITextComponent.h"
#include "Editor/AssetMetaManager.h"
#include "Editor/AssetPipelineManager.h"
#include "Editor/AsyncAssetLoader.h"
#include "Editor/EditorTransaction.h"
#include "GameObject/GameObject.h"
#include "GameObject/GameObjectRegistry.h"
#include "Generated/Prefab_generated.h"
#include "Generated/Scene_generated.h"
#include "Model/Model.h"
#include "Model/ModelResource.h"
#include "Scene/PrefabFlatBuffer.h"
#include "Scene/SceneFlatBuffer.h"
#include "System/EventBus.h"

namespace
{
    enum class TestCategory : uint32_t
    {
        Unit = 1u << 0,
        Integration = 1u << 1,
        RenderRegression = 1u << 2,
    };

    struct TestContext
    {
        int failures = 0;

        void expect(bool condition, const char* message)
        {
            if (!condition)
            {
                ++failures;
                LOG_ERROR("[Test] %s", message);
            }
        }

        void expectNear(float a, float b, float epsilon, const char* message)
        {
            expect(std::abs(a - b) <= epsilon, message);
        }
    };

    struct TestCase
    {
        const char* name = "";
        TestCategory category = TestCategory::Unit;
        std::function<void(TestContext&)> run;
    };

    using PixelBuffer = std::vector<uint8_t>;

    bool hasFlag(std::wstring_view args, std::wstring_view flag)
    {
        return args.find(flag) != std::wstring_view::npos;
    }

    uint32_t categoryMask(TestCategory category)
    {
        return static_cast<uint32_t>(category);
    }

    std::filesystem::path testTempRoot()
    {
        std::error_code ec;
        const auto base = std::filesystem::temp_directory_path(ec);
        if (ec)
        {
            return std::filesystem::path(".") / "TempTests";
        }

        return base / "DirectX12_tests";
    }

    void ensureCleanDir(const std::filesystem::path& dir)
    {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        ec.clear();
        std::filesystem::create_directories(dir, ec);
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

    bool writeTinyBmp(const std::filesystem::path& filePath, uint8_t red, uint8_t green, uint8_t blue)
    {
        const std::array<uint8_t, 58> bmp =
        {
            0x42, 0x4D,
            0x3A, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x36, 0x00, 0x00, 0x00,
            0x28, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00,
            0x01, 0x00,
            0x20, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x04, 0x00, 0x00, 0x00,
            0x13, 0x0B, 0x00, 0x00,
            0x13, 0x0B, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            blue, green, red, 0xFF
        };

        return writeFileBytes(filePath, bmp.data(), bmp.size());
    }

    bool readTextFile(const std::filesystem::path& filePath, std::string& outText)
    {
        std::ifstream in(filePath, std::ios::binary);
        if (!in)
        {
            return false;
        }

        outText.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return true;
    }

    bool writePrefabWithTextureDependency(const std::filesystem::path& prefabPath, const std::string& texturePath)
    {
        flatbuffers::FlatBufferBuilder builder(8 * 1024);

        const auto gpuPayload = scene::CreateGpuEffectComponentDataDirect(builder, texturePath.c_str(), 128u);
        const auto gpuComponent = scene::CreateSerializedComponentDirect(
            builder,
            "GpuEffectComponent",
            true,
            scene::ComponentPayload_GpuEffectComponentData,
            gpuPayload.Union());

        std::vector<flatbuffers::Offset<scene::SerializedComponent>> components;
        components.push_back(gpuComponent);

        std::vector<int32_t> tags;
        const auto object = scene::CreateSerializedGameObjectDirect(
            builder,
            "Root",
            true,
            -1,
            &tags,
            &components,
            nullptr);

        std::vector<flatbuffers::Offset<scene::SerializedGameObject>> objects;
        objects.push_back(object);

        const auto prefab = scene::CreateSerializedPrefabDirect(builder, 1, 0, &objects);
        scene::FinishSerializedPrefabBuffer(builder, prefab);

        return writeFileBytes(prefabPath, builder.GetBufferPointer(), builder.GetSize());
    }

    GameObject* findObjectByName(const std::vector<GameObject*>& objects, const std::string& name)
    {
        for (GameObject* object : objects)
        {
            if (object && !object->isDestroyed() && object->getName() == name)
            {
                return object;
            }
        }

        return nullptr;
    }

    std::shared_ptr<ModelResource> createMinimalAnimatedResource()
    {
        auto resource = DX_MAKE_SHARED(ModelResource);
        auto& modelData = resource->getModelData();

        ModelResource::Bone bone{};
        bone.name = "Root";
        bone.parentIndex = -1;
        bone.scale = Vector3(1.0f, 1.0f, 1.0f);
        bone.rotate = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        bone.translate = Vector3::Zero;
        modelData.bones.push_back(bone);

        ModelResource::Animation idle{};
        idle.name = "Idle";
        idle.secondsLength = 1.0f;

        ModelResource::Keyframe idleK0{};
        idleK0.seconds = 0.0f;
        idleK0.nodeKeys.push_back({ Vector3(1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f) });

        ModelResource::Keyframe idleK1{};
        idleK1.seconds = 1.0f;
        idleK1.nodeKeys.push_back({ Vector3(1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 1.0f), Vector3(0.1f, 0.0f, 0.0f) });

        idle.keyframes.push_back(idleK0);
        idle.keyframes.push_back(idleK1);

        ModelResource::Animation run{};
        run.name = "Run";
        run.secondsLength = 1.0f;

        ModelResource::Keyframe runK0{};
        runK0.seconds = 0.0f;
        runK0.nodeKeys.push_back({ Vector3(1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f) });

        ModelResource::Keyframe runK1{};
        runK1.seconds = 1.0f;
        runK1.nodeKeys.push_back({ Vector3(1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 1.0f), Vector3(2.0f, 0.0f, 0.0f) });

        run.keyframes.push_back(runK0);
        run.keyframes.push_back(runK1);

        modelData.animations.push_back(idle);
        modelData.animations.push_back(run);

        return resource;
    }

    struct ScriptEventProbe
    {
        int receiveCount = 0;
        int lastValue = 0;
        uint64_t lastSenderId = 0;
    };

    class TestListenerScript final : public ScriptComponent
    {
    public:

        explicit TestListenerScript(ScriptEventProbe* probe)
            : m_probe(probe)
        {
        }

        void awake() override
        {
            subscribeEvent(
                "Test.Event",
                [this](const Event& eventData)
                {
                    if (!m_probe)
                    {
                        return;
                    }

                    ++m_probe->receiveCount;
                    m_probe->lastSenderId = eventData.senderId;

                    if (const int* value = eventData.payloadAs<int>())
                    {
                        m_probe->lastValue = *value;
                    }
                });
        }

        void onDestroy() override
        {
            ScriptComponent::onDestroy();
        }

    private:

        ScriptEventProbe* m_probe = nullptr;
    };

    class TestEmitterScript final : public ScriptComponent
    {
    public:

        void emitValue(int value)
        {
            publishEvent("Test.Event", value);
        }

        void onDestroy() override
        {
            ScriptComponent::onDestroy();
        }
    };

    PixelBuffer generateReferencePattern(int width, int height)
    {
        PixelBuffer pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u, 0u);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const size_t index = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u;
                pixels[index + 0] = static_cast<uint8_t>((x * 13 + y * 7) & 0xFF);
                pixels[index + 1] = static_cast<uint8_t>((x * 5 + y * 17) & 0xFF);
                pixels[index + 2] = static_cast<uint8_t>(((x ^ y) * 9) & 0xFF);
            }
        }

        return pixels;
    }

    bool writePpm(const std::filesystem::path& path, int width, int height, const PixelBuffer& pixels)
    {
        std::ofstream out(path, std::ios::trunc);
        if (!out)
        {
            return false;
        }

        out << "P3\n";
        out << width << " " << height << "\n";
        out << "255\n";

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const size_t index = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u;
                out << static_cast<int>(pixels[index + 0]) << " "
                    << static_cast<int>(pixels[index + 1]) << " "
                    << static_cast<int>(pixels[index + 2]) << "\n";
            }
        }

        return out.good();
    }

    bool readPpm(const std::filesystem::path& path, int& width, int& height, PixelBuffer& outPixels)
    {
        std::ifstream in(path);
        if (!in)
        {
            return false;
        }

        std::string magic;
        int maxValue = 0;
        in >> magic >> width >> height >> maxValue;
        if (!in || magic != "P3" || width <= 0 || height <= 0 || maxValue != 255)
        {
            return false;
        }

        outPixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u, 0u);
        for (size_t i = 0; i < outPixels.size(); ++i)
        {
            int value = 0;
            in >> value;
            if (!in || value < 0 || value > 255)
            {
                return false;
            }
            outPixels[i] = static_cast<uint8_t>(value);
        }

        return true;
    }

    float computeMeanAbsDiff(const PixelBuffer& a, const PixelBuffer& b)
    {
        if (a.size() != b.size() || a.empty())
        {
            return std::numeric_limits<float>::infinity();
        }

        double total = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
        {
            total += std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i]));
        }

        return static_cast<float>(total / static_cast<double>(a.size()));
    }

    std::vector<TestCase> buildTests(bool updateGolden)
    {
        std::vector<TestCase> tests;

        tests.push_back({
            "Unit.AnimationStateMachine.TriggerTransition",
            TestCategory::Unit,
            [](TestContext& ctx)
            {
                auto resource = createMinimalAnimatedResource();
                Model model(resource);

                AnimationStateMachine sm;
                sm.addParameter("Go", AnimParamType::Trigger);

                AnimationState* idle = sm.addState("Idle", 0);
                sm.addState("Run", 1);

                AnimationTransition transition;
                transition.destStateName = "Run";
                transition.fadeDuration = 0.05f;
                transition.conditions.push_back({ "Go", CompareOp::Greater, 0.5f });
                idle->addTransition(transition);

                sm.setDefaultState("Idle");
                sm.initialize(&model);

                sm.update(0.016f);
                ctx.expect(sm.getCurrentStateName() == "Idle", "Expected Idle state before trigger");

                sm.setTrigger("Go");
                sm.update(0.016f);
                ctx.expect(sm.getCurrentStateName() == "Run", "Expected Run state after trigger");
                ctx.expect(sm.isFading(), "Expected fading after transition");
            }
        });

        tests.push_back({
            "Unit.PhysicsWorld.GravityRoundTrip",
            TestCategory::Unit,
            [](TestContext& ctx)
            {
                auto& world = PhysicsWorld::Instance();
                world.initialize();

                const Vector3 gravity(-1.0f, -3.5f, 2.0f);
                world.setGravity(gravity);

                const Vector3 actual = world.getGravity();
                ctx.expectNear(actual.x, gravity.x, 0.001f, "Gravity X mismatch");
                ctx.expectNear(actual.y, gravity.y, 0.001f, "Gravity Y mismatch");
                ctx.expectNear(actual.z, gravity.z, 0.001f, "Gravity Z mismatch");

                world.shutdown();
            }
        });

        tests.push_back({
            "Unit.RectTransformComponent.CalculateRect",
            TestCategory::Unit,
            [](TestContext& ctx)
            {
                RectTransformComponent rectTransform;
                rectTransform.setAnchor(Vector2(0.5f, 0.5f));
                rectTransform.setPosition(Vector2(10.0f, -20.0f));
                rectTransform.setSize(Vector2(200.0f, 80.0f));
                rectTransform.setPivot(Vector2(0.5f, 0.5f));

                const ImRect parent(ImVec2(100.0f, 50.0f), ImVec2(500.0f, 350.0f));
                const ImRect actual = rectTransform.calculateRect(parent);

                ctx.expectNear(actual.Min.x, 210.0f, 0.01f, "RectTransform min x mismatch");
                ctx.expectNear(actual.Min.y, 140.0f, 0.01f, "RectTransform min y mismatch");
                ctx.expectNear(actual.Max.x, 410.0f, 0.01f, "RectTransform max x mismatch");
                ctx.expectNear(actual.Max.y, 220.0f, 0.01f, "RectTransform max y mismatch");
            }
        });

        tests.push_back({
            "Unit.UIButtonComponent.PublishClickEvent",
            TestCategory::Unit,
            [](TestContext& ctx)
            {
                GameObjectRegistry::Instance().shutdown();
                EventBus::Instance().shutdown();

                UIButtonClickEvent genericPayload{};
                UIButtonClickEvent customPayload{};
                int genericCount = 0;
                int customCount = 0;

                const auto genericToken = EventBus::Instance().subscribe(
                    "UI.Button.Click",
                    [&](const EventBus::Event& eventData)
                    {
                        if (const auto* payload = eventData.payloadAs<UIButtonClickEvent>())
                        {
                            genericPayload = *payload;
                            ++genericCount;
                        }
                    });

                const auto customToken = EventBus::Instance().subscribe(
                    "UI.Menu.Start",
                    [&](const EventBus::Event& eventData)
                    {
                        if (const auto* payload = eventData.payloadAs<UIButtonClickEvent>())
                        {
                            customPayload = *payload;
                            ++customCount;
                        }
                    });

                GameObject* buttonObject = DX_NEW(GameObject, "StartButton");
                auto* button = buttonObject->addComponent<UIButtonComponent>();
                button->setClickEventName("UI.Menu.Start");

                ctx.expect(button->invokeClick(), "UIButton invokeClick should succeed");
                EventBus::Instance().dispatchQueued();

                ctx.expect(genericCount == 1, "UIButton should publish generic click event");
                ctx.expect(customCount == 1, "UIButton should publish named click event");
                ctx.expect(genericPayload.buttonObjectName == "StartButton", "UIButton generic payload name mismatch");
                ctx.expect(customPayload.eventName == "UI.Menu.Start", "UIButton custom payload event name mismatch");

                EventBus::Instance().unsubscribe(genericToken);
                EventBus::Instance().unsubscribe(customToken);
                GameObjectRegistry::Instance().shutdown();
                EventBus::Instance().shutdown();
            }
        });

        tests.push_back({
            "Unit.TimeManager.PlayPauseStep",
            TestCategory::Unit,
            [](TestContext& ctx)
            {
                TimeManager& time = TimeManager::Instance();
                time.initialize();
                time.update();

                time.pause();
                ctx.expect(time.isPaused(), "TimeManager pause should set paused state");

                time.update();
                ctx.expectNear(time.getDeltaTime(), 0.0f, 0.0001f, "Paused TimeManager should not advance delta time");

                time.requestSingleStep();
                time.update();
                ctx.expect(time.isPaused(), "TimeManager step should keep paused state");
                ctx.expect(time.getDeltaTime() > 0.0f, "TimeManager step should advance one frame");

                time.play();
                ctx.expect(!time.isPaused(), "TimeManager play should clear paused state");
            }
        });

        tests.push_back({
            "Unit.ScriptComponent.EventBusLifecycle",
            TestCategory::Unit,
            [](TestContext& ctx)
            {
                GameObjectRegistry::Instance().shutdown();

                ScriptEventProbe probe{};

                GameObject* emitterObject = DX_NEW(GameObject, "Emitter");
                auto* emitter = emitterObject->addComponent<TestEmitterScript>();

                GameObject* listenerObject = DX_NEW(GameObject, "Listener");
                listenerObject->addComponent<TestListenerScript>(&probe);

                emitter->emitValue(42);
                GameObjectRegistry::Instance().update();

                ctx.expect(probe.receiveCount == 1, "ScriptComponent listener did not receive event");
                ctx.expect(probe.lastValue == 42, "ScriptComponent listener payload mismatch");
                ctx.expect(probe.lastSenderId == emitterObject->getInstanceId(), "ScriptComponent listener sender mismatch");

                listenerObject->destroy();
                GameObjectRegistry::Instance().update();

                emitter->emitValue(7);
                GameObjectRegistry::Instance().update();

                ctx.expect(probe.receiveCount == 1, "Destroyed ScriptComponent should unsubscribe from EventBus");

                GameObjectRegistry::Instance().shutdown();
            }
        });

        tests.push_back({
            "Integration.SceneFlatBuffer.RoundTrip",
            TestCategory::Integration,
            [](TestContext& ctx)
            {
                const auto tempDir = testTempRoot() / "scene_roundtrip";
                ensureCleanDir(tempDir);
                const auto scenePath = tempDir / "scene.scn";

                GameObjectRegistry::Instance().shutdown();

                {
                    GameObject* root = DX_NEW(GameObject, "SceneRoot");
                    root->addTag(Tag::PostEffect);

                    GameObject* child = DX_NEW(GameObject, "SceneChild");
                    child->setParent(root);
                    child->setEnabled(false);

                    ctx.expect(SceneFlatBuffer::save(scenePath, SceneId::ParticleEditor), "Scene save failed");
                }

                GameObjectRegistry::Instance().shutdown();

                SceneId loadedId = SceneId::MAX;
                ctx.expect(SceneFlatBuffer::load(scenePath, SceneId::ParticleEditor, &loadedId), "Scene load failed");
                ctx.expect(loadedId == SceneId::ParticleEditor, "SceneId mismatch");

                const auto& objects = GameObjectRegistry::Instance().getAll();
                GameObject* root = findObjectByName(objects, "SceneRoot");
                GameObject* child = findObjectByName(objects, "SceneChild");

                ctx.expect(root != nullptr, "SceneRoot missing after load");
                ctx.expect(child != nullptr, "SceneChild missing after load");
                ctx.expect(root && root->hasTag(Tag::PostEffect), "SceneRoot tag mismatch");
                ctx.expect(child && child->getParent() == root, "SceneChild parent mismatch");
                ctx.expect(child && !child->isEnabled(), "SceneChild enabled mismatch");

                GameObjectRegistry::Instance().shutdown();
            }
        });

        tests.push_back({
            "Integration.Prefab.RoundTrip",
            TestCategory::Integration,
            [](TestContext& ctx)
            {
                const auto tempDir = testTempRoot() / "prefab_roundtrip";
                ensureCleanDir(tempDir);
                const auto prefabPath = tempDir / "sample.prefab";

                GameObjectRegistry::Instance().shutdown();

                {
                    GameObject* root = DX_NEW(GameObject, "PrefabRoot");
                    GameObject* child = DX_NEW(GameObject, "PrefabChild");
                    child->setParent(root);

                    ctx.expect(PrefabFlatBuffer::save(prefabPath, root), "Prefab save failed");
                }

                GameObjectRegistry::Instance().shutdown();

                GameObject* instanceRoot = PrefabFlatBuffer::instantiate(prefabPath);
                ctx.expect(instanceRoot != nullptr, "Prefab instantiate failed");

                const auto& objects = GameObjectRegistry::Instance().getAll();
                GameObject* instanceChild = findObjectByName(objects, "PrefabChild");

                ctx.expect(instanceRoot && instanceRoot->isPrefabInstanceRoot(), "Prefab root path missing");
                ctx.expect(instanceChild && instanceChild->getParent() == instanceRoot, "Prefab hierarchy mismatch");

                if (instanceRoot)
                {
                    instanceRoot->setName("PrefabRoot_Modified");
                    ctx.expect(PrefabFlatBuffer::apply(instanceRoot), "Prefab apply failed");

                    GameObject* reverted = PrefabFlatBuffer::revert(instanceRoot);
                    ctx.expect(reverted != nullptr, "Prefab revert failed");
                    ctx.expect(reverted && reverted->getName() == "PrefabRoot_Modified", "Prefab revert content mismatch");
                }

                GameObjectRegistry::Instance().shutdown();
            }
        });

        tests.push_back({
            "Integration.Physics.SyncFromSimulation",
            TestCategory::Integration,
            [](TestContext& ctx)
            {
                auto& world = PhysicsWorld::Instance();
                world.initialize();

                GameObjectRegistry::Instance().shutdown();

                GameObject* dynamicObj = DX_NEW(GameObject, "DynamicBody");
                auto* tf = dynamicObj->addComponent<TransformComponent>();
                tf->setPosition(Vector3(0.0f, 4.0f, 0.0f));

                dynamicObj->addComponent<ColliderComponent>();
                auto* rb = dynamicObj->addComponent<RigidbodyComponent>();
                rb->setType(RigidbodyType::Dynamic);

                dynamicObj->start();

                const float beforeY = tf->getPosition().y;
                for (int i = 0; i < 120; ++i)
                {
                    world.simulate(1.0f / 60.0f);
                }

                const float afterY = tf->getPosition().y;
                ctx.expect(afterY < beforeY - 0.1f, "RigidBody did not fall after simulation");

                GameObjectRegistry::Instance().shutdown();
                world.shutdown();
            }
        });

        tests.push_back({
            "Integration.Animation.TransitionAffectsPose",
            TestCategory::Integration,
            [](TestContext& ctx)
            {
                auto resource = createMinimalAnimatedResource();
                Model model(resource);

                AnimationStateMachine sm;
                sm.addParameter("Go", AnimParamType::Trigger);

                AnimationState* idle = sm.addState("Idle", 0);
                sm.addState("Run", 1);

                AnimationTransition transition;
                transition.destStateName = "Run";
                transition.fadeDuration = 0.01f;
                transition.conditions.push_back({ "Go", CompareOp::Greater, 0.5f });
                idle->addTransition(transition);

                sm.setDefaultState("Idle");
                sm.initialize(&model);

                sm.update(0.3f);
                const float beforeX = model.getBone().empty() ? 0.0f : model.getBone()[0].translate.x;

                sm.setTrigger("Go");
                sm.update(0.3f);
                sm.update(0.3f);

                const float afterX = model.getBone().empty() ? 0.0f : model.getBone()[0].translate.x;
                ctx.expect(sm.getCurrentStateName() == "Run", "Animation state did not transition to Run");
                ctx.expect(afterX > beforeX + 0.2f, "Animation pose did not move toward Run clip");
            }
        });

        tests.push_back({
            "Integration.AssetPipeline.DependencyAndIncrementalReimport",
            TestCategory::Integration,
            [](TestContext& ctx)
            {
                const auto root = testTempRoot() / "asset_pipeline_dep";
                ensureCleanDir(root);

                const auto dataRoot = root / "Data";
                const auto prefabDir = dataRoot / "Prefab";
                const auto textureDir = dataRoot / "Texture";
                std::filesystem::create_directories(prefabDir);
                std::filesystem::create_directories(textureDir);

                const auto texturePath = textureDir / "dep_tex.png";
                {
                    std::ofstream tex(texturePath, std::ios::binary | std::ios::trunc);
                    tex << "fake";
                }

                const auto prefabPath = prefabDir / "dep.prefab";
                const std::string textureRef = texturePath.generic_string();
                ctx.expect(writePrefabWithTextureDependency(prefabPath, textureRef), "Failed to write dependency prefab");

                auto& meta = EditorAssetMeta::AssetMetaManager::Instance();
                meta.clearCache();

                EditorAssetMeta::ReimportReport first{};
                ctx.expect(meta.refreshAsset(prefabPath, &first), "Initial refreshAsset failed");
                ctx.expect(first.reimported, "Expected first refresh to mark reimported");

                const auto deps = meta.getDependencies(prefabPath);
                ctx.expect(!deps.empty(), "Dependency graph should contain texture reference");

                const auto users = meta.getDependents(texturePath);
                bool hasPrefabAsDependent = false;
                for (const auto& user : users)
                {
                    if (user.generic_string().find("dep.prefab") != std::string::npos)
                    {
                        hasPrefabAsDependent = true;
                        break;
                    }
                }
                ctx.expect(hasPrefabAsDependent, "Reverse dependency graph missing prefab dependent");

                EditorAssetMeta::ReimportReport second{};
                ctx.expect(meta.refreshAsset(prefabPath, &second), "Second refreshAsset failed");
                ctx.expect(!second.reimported, "Second refresh should be incremental no-op");

                {
                    std::ofstream tex(texturePath, std::ios::binary | std::ios::trunc);
                    tex << "fake_changed";
                }

                EditorAssetMeta::ReimportReport third{};
                ctx.expect(meta.refreshAsset(prefabPath, &third), "Third refreshAsset failed");
                ctx.expect(third.dependencyChanged || third.reimported, "Dependency change should trigger reimport path");
            }
        });

        tests.push_back({
            "Integration.AssetPipeline.CookAndMigration",
            TestCategory::Integration,
            [](TestContext& ctx)
            {
                const auto root = testTempRoot() / "asset_pipeline_cook";
                ensureCleanDir(root);

                const auto sourceRoot = root / "Data";
                const auto prefabDir = sourceRoot / "Prefab";
                const auto textureDir = sourceRoot / "Texture";
                std::filesystem::create_directories(prefabDir);
                std::filesystem::create_directories(textureDir);

                const auto texturePath = textureDir / "cook_tex.png";
                {
                    std::ofstream tex(texturePath, std::ios::binary | std::ios::trunc);
                    tex << "cook_texture";
                }

                const auto prefabPath = prefabDir / "cook.prefab";
                ctx.expect(writePrefabWithTextureDependency(prefabPath, texturePath.generic_string()), "Failed to write cook prefab");

                // Create legacy meta format (without schema fields) and verify migration.
                const auto legacyMetaPath = texturePath.string() + ".meta";
                {
                    std::ofstream metaOut(legacyMetaPath, std::ios::trunc);
                    metaOut << "guid=legacy-guid\n";
                    metaOut << "importer=TextureImporter\n";
                    metaOut << "thumbnailMode=Auto\n";
                }

                auto& metaMgr = EditorAssetMeta::AssetMetaManager::Instance();
                metaMgr.clearCache();

                EditorAssetMeta::ReimportReport migrationReport{};
                ctx.expect(metaMgr.refreshAsset(texturePath, &migrationReport), "Legacy meta refresh failed");

                std::string migratedMetaText;
                ctx.expect(readTextFile(legacyMetaPath, migratedMetaText), "Failed to read migrated meta");
                ctx.expect(migratedMetaText.find("schemaVersion=") != std::string::npos, "Migrated meta missing schemaVersion");
                ctx.expect(migratedMetaText.find("importerVersion=") != std::string::npos, "Migrated meta missing importerVersion");

                EditorAssetMeta::CookReport cookReport{};
                const auto cookRoot = root / "CookedData";
                ctx.expect(metaMgr.cookAssets(sourceRoot, cookRoot, &cookReport), "cookAssets failed");
                ctx.expect(cookReport.cookedAssetCount >= 2, "Cooked asset count should include prefab and texture");
                ctx.expect(cookReport.copiedFileCount >= 2, "Cook should copy prefab and texture");
                ctx.expect(std::filesystem::exists(cookReport.manifestPath), "Cook manifest file missing");

                std::string manifestText;
                ctx.expect(readTextFile(cookReport.manifestPath, manifestText), "Failed to read cook manifest");
                ctx.expect(manifestText.find("manifestVersion=") != std::string::npos, "Cook manifest missing version");
                ctx.expect(manifestText.find("dependency=") != std::string::npos, "Cook manifest missing dependency entries");
            }
        });

        tests.push_back({
            "Integration.AssetPipeline.PakAndPatch",
            TestCategory::Integration,
            [](TestContext& ctx)
            {
                const auto root = testTempRoot() / "asset_pipeline_pak";
                ensureCleanDir(root);

                const auto sourceRoot = root / "Data";
                const auto textureDir = sourceRoot / "Texture";
                const auto prefabDir = sourceRoot / "Prefab";
                std::filesystem::create_directories(textureDir);
                std::filesystem::create_directories(prefabDir);

                const auto texturePath = textureDir / "pak_tex.bmp";
                ctx.expect(writeTinyBmp(texturePath, 255, 64, 32), "Failed to write initial BMP texture");

                const auto prefabPath = prefabDir / "pak.prefab";
                ctx.expect(writePrefabWithTextureDependency(prefabPath, texturePath.generic_string()), "Failed to write pak prefab");

                auto& pipeline = EditorAssetPipeline::AssetPipelineManager::Instance();

                EditorAssetPipeline::CookSettings cookSettings{};
                cookSettings.sourceRoot = sourceRoot;
                cookSettings.cookRoot = root / "CookedData";
                cookSettings.texture.target = EditorAssetPipeline::TextureTarget::BC7;

                EditorAssetPipeline::CookReport cookReport{};
                ctx.expect(pipeline.cookAssets(cookSettings, &cookReport), "Initial BC7 cook failed");
                ctx.expect(cookReport.textureCount >= 1, "Cook should recognize the BMP texture");

                const auto cookedTexture = cookSettings.cookRoot / "Texture" / "pak_tex.dds";
                ctx.expect(std::filesystem::exists(cookedTexture), "Cooked BC7 texture missing");

                EditorAssetPipeline::PackageReport packageReport{};
                const auto pakPath = root / "Package" / "Game.pak";
                ctx.expect(pipeline.buildPak(cookSettings.cookRoot, pakPath, &packageReport), "Initial pak build failed");
                ctx.expect(packageReport.entryCount >= 2, "Pak should contain prefab and texture entries");
                ctx.expect(std::filesystem::exists(packageReport.manifestPath), "Pak manifest missing");

                std::string pakManifestText;
                ctx.expect(readTextFile(packageReport.manifestPath, pakManifestText), "Failed to read pak manifest");
                ctx.expect(pakManifestText.find("entry=") != std::string::npos, "Pak manifest missing entry rows");

                const auto cookRootV2 = root / "CookedDataV2";
                ctx.expect(writeTinyBmp(texturePath, 16, 200, 240), "Failed to update BMP texture");

                EditorAssetPipeline::CookSettings cookSettingsV2 = cookSettings;
                cookSettingsV2.cookRoot = cookRootV2;

                EditorAssetPipeline::CookReport cookReportV2{};
                ctx.expect(pipeline.cookAssets(cookSettingsV2, &cookReportV2), "Updated BC7 cook failed");

                EditorAssetPipeline::PackageReport packageReportV2{};
                const auto pakPathV2 = root / "Package" / "GameV2.pak";
                ctx.expect(pipeline.buildPak(cookSettingsV2.cookRoot, pakPathV2, &packageReportV2), "Updated pak build failed");

                EditorAssetPipeline::PatchReport patchReport{};
                const auto patchPath = root / "Package" / "Game.patchpak";
                ctx.expect(pipeline.buildPatch(pakPath, pakPathV2, patchPath, &patchReport), "Patch build failed");
                ctx.expect(patchReport.changedCount >= 1, "Patch should contain at least one changed asset");
                ctx.expect(std::filesystem::exists(patchReport.manifestPath), "Patch manifest missing");

                std::string patchManifestText;
                ctx.expect(readTextFile(patchReport.manifestPath, patchManifestText), "Failed to read patch manifest");
                ctx.expect(patchManifestText.find("changed=") != std::string::npos, "Patch manifest missing changed rows");
            }
        });

        tests.push_back({
            "Integration.AssetPipeline.UnicodePathMetaRefresh",
            TestCategory::Integration,
            [](TestContext& ctx)
            {
                const auto root = testTempRoot() / "asset_pipeline_unicode";
                ensureCleanDir(root);

                const auto dataRoot = root / "Data";
                const auto prefabDir = dataRoot / "Prefab";
                const auto unicodeTextureDir = dataRoot / std::filesystem::path(L"\u5343\u590F");
                std::filesystem::create_directories(prefabDir);
                std::filesystem::create_directories(unicodeTextureDir);

                const auto texturePath = unicodeTextureDir / std::filesystem::path(L"\u5730\u5F62.png");
                {
                    std::ofstream tex(texturePath, std::ios::binary | std::ios::trunc);
                    tex << "unicode_texture";
                }

                const auto prefabPath = prefabDir / "unicode_dep.prefab";
                const std::string textureRef = texturePath.generic_string();
                ctx.expect(writePrefabWithTextureDependency(prefabPath, textureRef), "Failed to write unicode dependency prefab");

                auto& meta = EditorAssetMeta::AssetMetaManager::Instance();
                meta.clearCache();

                EditorAssetMeta::ReimportReport textureReport{};
                ctx.expect(meta.refreshAsset(texturePath, &textureReport), "Unicode texture refreshAsset failed");

                const std::filesystem::path textureMetaPath = std::filesystem::path(texturePath.native() + std::filesystem::path::string_type(L".meta"));
                ctx.expect(std::filesystem::exists(textureMetaPath), "Unicode texture meta file was not created");

                EditorAssetMeta::ReimportReport prefabReport{};
                ctx.expect(meta.refreshAsset(prefabPath, &prefabReport), "Unicode prefab refreshAsset failed");

                const auto deps = meta.getDependencies(prefabPath);
                bool containsUnicodeTexture = false;
                for (const auto& dep : deps)
                {
                    if (dep.generic_string().find(".png") != std::string::npos && dep.generic_string().find("\xE5\x8D\x83\xE5\xA4\x8F") != std::string::npos)
                    {
                        containsUnicodeTexture = true;
                        break;
                    }
                }
                ctx.expect(containsUnicodeTexture, "Dependency graph should include unicode texture path");
            }
        });

        tests.push_back({
            "Unit.EditorTransaction.UndoRedo",
            TestCategory::Unit,
            [](TestContext& ctx)
            {
                auto& transactions = EditorTransaction::Manager::Instance();
                transactions.clear();

                int value = 0;
                value = 42;

                transactions.record(
                    "Set Value",
                    [&value]() { value = 0; },
                    [&value]() { value = 42; });

                ctx.expect(transactions.canUndo(), "Transaction stack should have undo item");
                ctx.expect(transactions.undo(), "Undo should succeed");
                ctx.expect(value == 0, "Undo should restore previous value");

                ctx.expect(transactions.canRedo(), "Transaction stack should have redo item");
                ctx.expect(transactions.redo(), "Redo should succeed");
                ctx.expect(value == 42, "Redo should re-apply value");
            }
        });

        tests.push_back({
            "Unit.EditorTransaction.CompositeAndRedoInvalidation",
            TestCategory::Unit,
            [](TestContext& ctx)
            {
                auto& transactions = EditorTransaction::Manager::Instance();
                transactions.clear();

                int a = 0;
                int b = 0;

                transactions.begin("Move Pair");
                transactions.addStep(
                    [&a]() { a = 0; },
                    [&a]() { a = 10; });
                transactions.addStep(
                    [&b]() { b = 0; },
                    [&b]() { b = 20; });

                a = 10;
                b = 20;
                transactions.commit();

                ctx.expect(std::string(transactions.nextUndoLabel()) == "Move Pair", "Undo label should match committed transaction");
                ctx.expect(transactions.undo(), "Composite undo should succeed");
                ctx.expect(a == 0 && b == 0, "Composite undo should restore both values");
                ctx.expect(transactions.canRedo(), "Redo should be available after undo");

                transactions.record(
                    "Set A",
                    [&a]() { a = 0; },
                    [&a]() { a = 1; });

                a = 1;

                ctx.expect(!transactions.canRedo(), "Redo stack must be cleared when a new command is recorded");
            }
        });

        tests.push_back({
            "Integration.Prefab.VariantOverrideInfo",
            TestCategory::Integration,
            [](TestContext& ctx)
            {
                const auto tempDir = testTempRoot() / "prefab_variant_override";
                ensureCleanDir(tempDir);

                const auto basePath = tempDir / "base.prefab";
                const auto variantPath = tempDir / "variant.prefab";

                GameObjectRegistry::Instance().shutdown();

                GameObject* sourceRoot = DX_NEW(GameObject, "BaseRoot");
                sourceRoot->addComponent<TransformComponent>();
                sourceRoot->start();

                ctx.expect(PrefabFlatBuffer::save(basePath, sourceRoot), "Base prefab save failed");
                GameObjectRegistry::Instance().shutdown();

                GameObject* instance = PrefabFlatBuffer::instantiate(basePath);
                ctx.expect(instance != nullptr, "Base prefab instantiate failed");
                ctx.expect(instance && instance->isPrefabInstanceRoot(), "Instance should be prefab root");

                if (instance)
                {
                    instance->setName("BaseRoot_Override");

                    PrefabFlatBuffer::OverrideInfo info{};
                    ctx.expect(PrefabFlatBuffer::buildOverrideInfo(instance, info), "Override info build failed");
                    ctx.expect(info.valid, "Override info should be valid");
                    ctx.expect(!info.entries.empty(), "Override entries should not be empty after rename override");

                    ctx.expect(PrefabFlatBuffer::createVariant(variantPath, basePath, instance), "Create variant failed");
                    GameObjectRegistry::Instance().shutdown();

                    GameObject* variantInstance = PrefabFlatBuffer::instantiate(variantPath);
                    ctx.expect(variantInstance != nullptr, "Variant instantiate failed");

                    if (variantInstance)
                    {
                        PrefabFlatBuffer::OverrideInfo variantInfo{};
                        ctx.expect(PrefabFlatBuffer::buildOverrideInfo(variantInstance, variantInfo), "Variant override info build failed");
                        ctx.expect(variantInfo.valid, "Variant override info should be valid");
                        ctx.expect(variantInfo.isVariant, "Variant instance should be marked as variant");
                        ctx.expect(!variantInfo.basePrefabPath.empty(), "Variant base prefab path should be detected");
                    }
                }

                GameObjectRegistry::Instance().shutdown();
            }
        });

        tests.push_back({
            "Integration.AsyncLoading.SceneAndAsset",
            TestCategory::Integration,
            [](TestContext& ctx)
            {
                const auto tempDir = testTempRoot() / "async_loading";
                ensureCleanDir(tempDir);

                const auto scenePath = tempDir / "async_scene.scn";
                const auto prefabPath = tempDir / "async_prefab.prefab";

                GameObjectRegistry::Instance().shutdown();
                EditorAsyncAsset::AsyncAssetLoader::Instance().clear();

                {
                    GameObject* sceneRoot = DX_NEW(GameObject, "AsyncSceneRoot");
                    sceneRoot->addComponent<TransformComponent>();
                    ctx.expect(SceneFlatBuffer::save(scenePath, SceneId::ParticleEditor), "Async scene save failed");
                }

                GameObjectRegistry::Instance().shutdown();

                {
                    GameObject* prefabRoot = DX_NEW(GameObject, "AsyncPrefabRoot");
                    prefabRoot->addComponent<TransformComponent>();
                    ctx.expect(PrefabFlatBuffer::save(prefabPath, prefabRoot), "Async prefab save failed");
                }

                GameObjectRegistry::Instance().shutdown();

                auto& loader = EditorAsyncAsset::AsyncAssetLoader::Instance();

                ctx.expect(loader.enqueueScene(scenePath), "enqueueScene failed");

                bool sceneLoaded = false;
                for (int i = 0; i < 300 && !sceneLoaded; ++i)
                {
                    loader.update();
                    SceneManager::Instance().update();

                    const auto& objects = GameObjectRegistry::Instance().getAll();
                    sceneLoaded = findObjectByName(objects, "AsyncSceneRoot") != nullptr;
                    if (!sceneLoaded)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
                ctx.expect(sceneLoaded, "Async scene load did not complete in time");

                ctx.expect(loader.enqueuePrefab(prefabPath, nullptr), "enqueuePrefab failed");

                bool prefabLoaded = false;
                for (int i = 0; i < 300 && !prefabLoaded; ++i)
                {
                    loader.update();
                    SceneManager::Instance().update();

                    const auto& objects = GameObjectRegistry::Instance().getAll();
                    prefabLoaded = findObjectByName(objects, "AsyncPrefabRoot") != nullptr;
                    if (!prefabLoaded)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
                ctx.expect(prefabLoaded, "Async prefab load did not complete in time");

                loader.clear();
                GameObjectRegistry::Instance().shutdown();
            }
        });

        tests.push_back({
            "RenderRegression.GoldenPattern.PPM",
            TestCategory::RenderRegression,
            [updateGolden](TestContext& ctx)
            {
                constexpr int width = 32;
                constexpr int height = 32;

                const auto goldenPath = std::filesystem::path("Data") / "TestGolden" / "RenderPattern.ppm";
                std::error_code ec;
                std::filesystem::create_directories(goldenPath.parent_path(), ec);

                const PixelBuffer current = generateReferencePattern(width, height);

                if (updateGolden || !std::filesystem::exists(goldenPath))
                {
                    const bool writeOk = writePpm(goldenPath, width, height, current);
                    ctx.expect(writeOk, "Failed to update render golden file");
                    if (!writeOk)
                    {
                        return;
                    }
                }

                int goldenWidth = 0;
                int goldenHeight = 0;
                PixelBuffer goldenPixels;

                const bool readOk = readPpm(goldenPath, goldenWidth, goldenHeight, goldenPixels);
                ctx.expect(readOk, "Failed to read render golden file");
                if (!readOk)
                {
                    return;
                }

                ctx.expect(goldenWidth == width && goldenHeight == height, "Golden image dimensions mismatch");
                const float meanDiff = computeMeanAbsDiff(current, goldenPixels);
                ctx.expect(meanDiff <= 0.0f, "Golden image comparison failed");

                const auto outputPath = testTempRoot() / "render_regression" / "RenderPattern.current.ppm";
                std::filesystem::create_directories(outputPath.parent_path(), ec);
                writePpm(outputPath, width, height, current);
            }
        });

        return tests;
    }

    int runTests(uint32_t selectedMask, bool updateGolden)
    {
        const auto tests = buildTests(updateGolden);

        int totalRun = 0;
        int totalFailed = 0;

        for (const auto& test : tests)
        {
            if ((selectedMask & categoryMask(test.category)) == 0)
            {
                continue;
            }

            ++totalRun;
            LOG_INFO("[Test] RUN %s", test.name);

            TestContext context{};
            test.run(context);

            if (context.failures > 0)
            {
                totalFailed += context.failures;
                LOG_ERROR("[Test] FAIL %s (%d failures)", test.name, context.failures);
            }
            else
            {
                LOG_INFO("[Test] PASS %s", test.name);
            }
        }

        LOG_INFO("[Test] Summary: run=%d failed=%d", totalRun, totalFailed);
        return totalFailed == 0 ? 0 : 1;
    }
}

namespace EngineTests
{
    bool hasAnyTestFlag(LPWSTR cmdLine)
    {
        const std::wstring_view args = cmdLine ? std::wstring_view(cmdLine) : std::wstring_view();

        return hasFlag(args, L"--ci-smoke")
            || hasFlag(args, L"--test-all")
            || hasFlag(args, L"--test-unit")
            || hasFlag(args, L"--test-integration")
            || hasFlag(args, L"--test-render-regression");
    }

    int runFromCommandLine(LPWSTR cmdLine)
    {
        const std::wstring_view args = cmdLine ? std::wstring_view(cmdLine) : std::wstring_view();

        const bool runCiSmoke = hasFlag(args, L"--ci-smoke");
        const bool runAll = hasFlag(args, L"--test-all");
        const bool runUnit = hasFlag(args, L"--test-unit");
        const bool runIntegration = hasFlag(args, L"--test-integration");
        const bool runRender = hasFlag(args, L"--test-render-regression");
        const bool updateGolden = hasFlag(args, L"--update-golden");

        uint32_t selectedMask = 0;

        if (runCiSmoke)
        {
            selectedMask = categoryMask(TestCategory::Unit)
                | categoryMask(TestCategory::Integration)
                | categoryMask(TestCategory::RenderRegression);
        }
        else
        {
            if (runAll)
            {
                selectedMask |= categoryMask(TestCategory::Unit)
                    | categoryMask(TestCategory::Integration)
                    | categoryMask(TestCategory::RenderRegression);
            }
            if (runUnit)
            {
                selectedMask |= categoryMask(TestCategory::Unit);
            }
            if (runIntegration)
            {
                selectedMask |= categoryMask(TestCategory::Integration);
            }
            if (runRender)
            {
                selectedMask |= categoryMask(TestCategory::RenderRegression);
            }
        }

        if (selectedMask == 0)
        {
            selectedMask = categoryMask(TestCategory::Unit)
                | categoryMask(TestCategory::Integration)
                | categoryMask(TestCategory::RenderRegression);
        }

        return runTests(selectedMask, updateGolden);
    }
}
