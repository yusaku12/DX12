#include "pch.h"
#include "TestScene.h"
#include "GameObject\GameObject.h"
#include "Component\TransformComponent.h"
#include "Component\RigidbodyComponent.h"
#include "Component\ColliderComponent.h"

void TestScene::onEnter()
{
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
}

void TestScene::drawMultiThreaded()
{
}

void TestScene::debugDraw()
{
    //m_pmxRender->debugRender();
    //m_fbxRender->debugRender();

    //if (!ImGui::Begin("MT Command Recording"))
    //{
    //    ImGui::End();
    //    return;
    //}

    //float speedup = (m_totalMultiThreadMs > 0.001f)
    //    ? m_singleThreadEstimateMs / m_totalMultiThreadMs
    //    : 0.0f;

    //ImGui::Text("Multi-thread total : %.3f ms", m_totalMultiThreadMs);
    //ImGui::Text("Single-thread est. : %.3f ms", m_singleThreadEstimateMs);
    //ImGui::Text("Speedup            : %.2fx", speedup);
    //ImGui::Separator();

    //ImGui::Text("Thread Timeline:");

    //float maxTime = m_totalMultiThreadMs;
    //if (maxTime < 0.001f) maxTime = 1.0f;

    //ImDrawList* drawList = ImGui::GetWindowDrawList();
    //ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    //float canvasWidth = ImGui::GetContentRegionAvail().x;
    //float barHeight = 24.0f;
    //float padding = 4.0f;

    //ImU32 colors[] =
    //{
    //    IM_COL32(66, 135, 245, 255),
    //    IM_COL32(245, 166, 35, 255),
    //    IM_COL32(80, 200, 120, 255),
    //    IM_COL32(220, 80, 80, 255),
    //};

    //for (size_t i = 0; i < m_threadTimings.size(); ++i)
    //{
    //    const auto& t = m_threadTimings[i];

    //    float x0 = canvasPos.x + (t.startMs / maxTime) * canvasWidth;
    //    float x1 = canvasPos.x + ((t.startMs + t.durationMs) / maxTime) * canvasWidth;
    //    float y0 = canvasPos.y + i * (barHeight + padding);
    //    float y1 = y0 + barHeight;

    //    ImU32 color = colors[i % 4];

    //    drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color, 4.0f);

    //    char label[128];
    //    std::snprintf(label, sizeof(label), "%s  %.2f ms (TID: %zu)",
    //        t.name, t.durationMs,
    //        std::hash<std::thread::id>{}(t.threadId) % 10000);

    //    drawList->AddText(ImVec2(x0 + 4.0f, y0 + 4.0f), IM_COL32(255, 255, 255, 255), label);
    //}

    //float totalHeight = static_cast<float>(m_threadTimings.size()) * (barHeight + padding) + padding;
    //ImGui::Dummy(ImVec2(canvasWidth, totalHeight));

    //ImGui::Separator();
    //if (ImGui::BeginTable("ThreadDetails", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    //{
    //    ImGui::TableSetupColumn("Task");
    //    ImGui::TableSetupColumn("Start (ms)");
    //    ImGui::TableSetupColumn("Duration (ms)");
    //    ImGui::TableSetupColumn("Thread ID");
    //    ImGui::TableHeadersRow();

    //    for (const auto& t : m_threadTimings)
    //    {
    //        ImGui::TableNextRow();
    //        ImGui::TableSetColumnIndex(0);
    //        ImGui::Text("%s", t.name);
    //        ImGui::TableSetColumnIndex(1);
    //        ImGui::Text("%.3f", t.startMs);
    //        ImGui::TableSetColumnIndex(2);
    //        ImGui::Text("%.3f", t.durationMs);
    //        ImGui::TableSetColumnIndex(3);
    //        ImGui::Text("%zu", std::hash<std::thread::id>{}(t.threadId) % 10000);
    //    }
    //    ImGui::EndTable();
    //}

    //ImGui::End();
}