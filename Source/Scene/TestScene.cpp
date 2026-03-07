#include "pch.h"
#include "TestScene.h"
#include "GameObject\GameObject.h"
#include "Component\TransformComponent.h"
#include "Component\RigidbodyComponent.h"
#include "Component\ColliderComponent.h"

void TestScene::onEnter()
{
    //! PMX
    {
        GameObject* modelObj = new GameObject("PMX_Model_Object");
        TransformComponent* tf = modelObj->addComponent<TransformComponent>();
        tf->setPosition(Vector3(-7.0f, 0.0f, 0.0f));
        tf->setRotation(Quaternion::Identity);
        tf->setScale(Vector3::One);

        m_pmxRender = std::make_unique<PMXRender>(L"Data/Model/千夏/千夏皮肤.pmx");
        m_pmxRender->setTransform(tf);
    }

    //! FBX
    {
        GameObject* modelObj = new GameObject("FBX_Model_Object");
        TransformComponent* tf = modelObj->addComponent<TransformComponent>();
        tf->setPosition(Vector3(7.0f, 0.0f, 0.0f));
        tf->setRotation(Quaternion::Identity);
        tf->setScale(Vector3::One);

        m_fbxRender = std::make_unique<FBXRender>("Data/Model/Jammo/Jammo.fbx");
        m_fbxRender->setTransform(tf);
    }

    //! 地面（Static + PlaneCollider）
    {
        GameObject* ground = new GameObject("Ground");
        TransformComponent* tf = ground->addComponent<TransformComponent>();
        tf->setPosition({ 0, 0, 0 });
        tf->setRotation(Quaternion::Identity);

        ColliderComponent* col = ground->addComponent<ColliderComponent>();
        col->setPlaneShape();

        RigidbodyComponent* rb = ground->addComponent<RigidbodyComponent>();
        rb->setType(RigidbodyType::Static);
    }

    //! 落下する球体（Dynamic + SphereCollider）
    {
        GameObject* sphere = new GameObject("Physics_Sphere");
        TransformComponent* tf = sphere->addComponent<TransformComponent>();
        tf->setPosition(Vector3(0.0f, 10.0f, 0.0f));

        ColliderComponent* col = sphere->addComponent<ColliderComponent>();
        col->setSphereShape(0.5f);

        RigidbodyComponent* rb = sphere->addComponent<RigidbodyComponent>();
        rb->setMass(1.0f);
    }

    //! 落下するボックス（Dynamic + BoxCollider）
    {
        GameObject* box = new GameObject("Physics_Box");
        TransformComponent* tf = box->addComponent<TransformComponent>();
        tf->setPosition(Vector3(2.0f, 5.0f, 0.0f));

        ColliderComponent* col = box->addComponent<ColliderComponent>();
        col->setBoxShape(Vector3(0.5f, 0.5f, 0.5f));

        RigidbodyComponent* rb = box->addComponent<RigidbodyComponent>();
        rb->setMass(2.0f);
    }
}

void TestScene::onExit()
{
}

void TestScene::update()
{
    //! グリッドは毎フレーム描画リクエストを出す
    DebugPrimitive::Instance().drawGrid({ 0.0f,0.0f,0.0f }, 100.0f, 100.0f, 1.0f, { 0.5f,0.5f,0.5f,1.0f });
}

void TestScene::draw()
{
    m_pmxRender->render();
    m_fbxRender->render();
}

void TestScene::drawMultiThreaded()
{
    using Clock = std::chrono::high_resolution_clock;

    auto& dx12 = DX12::Instance();
    auto& pool = CommandListPool::Instance();

    m_frameStart = Clock::now();
    m_threadTimings.clear();

    auto* cmdPmx = pool.acquire();
    auto* cmdFbx = pool.acquire();

    dx12.applyViewportAndScissor(cmdPmx);
    dx12.applySceneRenderTargets(cmdPmx);
    dx12.applyViewportAndScissor(cmdFbx);
    dx12.applySceneRenderTargets(cmdFbx);

    auto futurePmx = std::async(std::launch::async, [this, cmdPmx]()
        {
            using Clock = std::chrono::high_resolution_clock;
            auto start = Clock::now();

            m_pmxRender->render(cmdPmx);

            auto end = Clock::now();
            float startMs = std::chrono::duration<float, std::milli>(start - m_frameStart).count();
            float durationMs = std::chrono::duration<float, std::milli>(end - start).count();

            std::lock_guard<std::mutex> lock(m_timingMutex);
            m_threadTimings.push_back({ "PMX", startMs, durationMs, std::this_thread::get_id() });
        });

    auto futureFbx = std::async(std::launch::async, [this, cmdFbx]()
        {
            using Clock = std::chrono::high_resolution_clock;
            auto start = Clock::now();

            m_fbxRender->render(cmdFbx);

            auto end = Clock::now();
            float startMs = std::chrono::duration<float, std::milli>(start - m_frameStart).count();
            float durationMs = std::chrono::duration<float, std::milli>(end - start).count();

            std::lock_guard<std::mutex> lock(m_timingMutex);
            m_threadTimings.push_back({ "FBX", startMs, durationMs, std::this_thread::get_id() });
        });

    futurePmx.wait();
    futureFbx.wait();

    m_totalMultiThreadMs = std::chrono::duration<float, std::milli>(Clock::now() - m_frameStart).count();

    m_singleThreadEstimateMs = 0.0f;
    for (const auto& t : m_threadTimings)
        m_singleThreadEstimateMs += t.durationMs;

    pool.release(cmdPmx);
    pool.release(cmdFbx);
}

void TestScene::debugDraw()
{
    m_pmxRender->debugRender();
    m_fbxRender->debugRender();

    if (!ImGui::Begin("MT Command Recording"))
    {
        ImGui::End();
        return;
    }

    float speedup = (m_totalMultiThreadMs > 0.001f)
        ? m_singleThreadEstimateMs / m_totalMultiThreadMs
        : 0.0f;

    ImGui::Text("Multi-thread total : %.3f ms", m_totalMultiThreadMs);
    ImGui::Text("Single-thread est. : %.3f ms", m_singleThreadEstimateMs);
    ImGui::Text("Speedup            : %.2fx", speedup);
    ImGui::Separator();

    ImGui::Text("Thread Timeline:");

    float maxTime = m_totalMultiThreadMs;
    if (maxTime < 0.001f) maxTime = 1.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    float canvasWidth = ImGui::GetContentRegionAvail().x;
    float barHeight = 24.0f;
    float padding = 4.0f;

    ImU32 colors[] =
    {
        IM_COL32(66, 135, 245, 255),
        IM_COL32(245, 166, 35, 255),
        IM_COL32(80, 200, 120, 255),
        IM_COL32(220, 80, 80, 255),
    };

    for (size_t i = 0; i < m_threadTimings.size(); ++i)
    {
        const auto& t = m_threadTimings[i];

        float x0 = canvasPos.x + (t.startMs / maxTime) * canvasWidth;
        float x1 = canvasPos.x + ((t.startMs + t.durationMs) / maxTime) * canvasWidth;
        float y0 = canvasPos.y + i * (barHeight + padding);
        float y1 = y0 + barHeight;

        ImU32 color = colors[i % 4];

        drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color, 4.0f);

        char label[128];
        std::snprintf(label, sizeof(label), "%s  %.2f ms (TID: %zu)",
            t.name, t.durationMs,
            std::hash<std::thread::id>{}(t.threadId) % 10000);

        drawList->AddText(ImVec2(x0 + 4.0f, y0 + 4.0f), IM_COL32(255, 255, 255, 255), label);
    }

    float totalHeight = static_cast<float>(m_threadTimings.size()) * (barHeight + padding) + padding;
    ImGui::Dummy(ImVec2(canvasWidth, totalHeight));

    ImGui::Separator();
    if (ImGui::BeginTable("ThreadDetails", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Task");
        ImGui::TableSetupColumn("Start (ms)");
        ImGui::TableSetupColumn("Duration (ms)");
        ImGui::TableSetupColumn("Thread ID");
        ImGui::TableHeadersRow();

        for (const auto& t : m_threadTimings)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", t.name);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", t.startMs);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", t.durationMs);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%zu", std::hash<std::thread::id>{}(t.threadId) % 10000);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}