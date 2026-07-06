#include "pch.h"
#include "Editor/EditorManager.h"
#include "Render/RenderPipeline.h"
#include "Render/RenderManager.h"
#include "Render/GBufferRenderTargets.h"
#include "AssetBrowserWindow.h"
#include "AsyncAssetLoader.h"
#include "BehaviorTreeGraphEditorWindow.h"
#include "MaterialGraphEditorWindow.h"
#include "EditorTransaction.h"
#include "HierarchyWindow.h"
#include "InspectorWindow.h"
#include "CubemapToolWindow.h"
#include "NavMeshBakeWindow.h"
#include "GameObject\ObjectPicker.h"
#include "Graphics\DX12.h"
#include "Render\GBufferRenderTargets.h"
#include "Render\HiZPyramid.h"
#include "Render\RenderManager.h"
#include "Render\RenderPipeline.h"
#include "Scene\SceneManager.h"
#include "Audio/AudioManager.h"
#include "System\Logger.h"
#include "System\RuntimeUIManager.h"
#include "System\TimeManager.h"

namespace
{
    constexpr const char* kDockspaceWindowName = "EditorRootDockspace";
    constexpr const char* kSceneWindowName = "Scene";
    constexpr const char* kHierarchyWindowName = "Hierarchy";
    constexpr const char* kInspectorWindowName = "Inspector";
    constexpr const char* kProjectWindowName = "Project";
    constexpr const char* kDebugHubWindowName = "Debug Hub";
    constexpr const char* kSceneSettingsWindowName = "Scene Settings";
}

void EditorManager::update()
{
    EditorAsyncAsset::AsyncAssetLoader::Instance().update();

    ImGuiIO& io = ImGui::GetIO();
    // テキスト入力中はショートカットを無効にして、入力競合を回避する。
    if (!io.WantTextInput)
    {
        const bool ctrl = io.KeyCtrl;
        const bool shift = io.KeyShift;

        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            EditorTransaction::Manager::Instance().undo();
        }
        else if ((ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
            || (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)))
        {
            EditorTransaction::Manager::Instance().redo();
        }
    }

    // Scene ウィンドウ上でのオブジェクトピッキング
    if (m_windowState.scene)
    {
        ObjectPicker::Instance().update();
    }
}

void EditorManager::imgui()
{
    drawDockspaceHost();
    drawManagedWindows();
}

void EditorManager::drawDockspaceHost()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin(kDockspaceWindowName, nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    drawMainMenuBar();

    ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (m_requestLayoutReset || !m_layoutInitialized)
    {
        applyDefaultLayout(dockspaceId);
        m_requestLayoutReset = false;
        m_layoutInitialized = true;
    }

    ImGui::End();
}

void EditorManager::drawMainMenuBar()
{
    if (!ImGui::BeginMenuBar())
    {
        return;
    }

    {
        auto& transactions = EditorTransaction::Manager::Instance();
        const bool canUndo = transactions.canUndo();
        const bool canRedo = transactions.canRedo();
        std::string undoLabel = "Undo";
        std::string redoLabel = "Redo";

        if (canUndo)
        {
            undoLabel += " ";
            undoLabel += transactions.nextUndoLabel();
        }

        if (canRedo)
        {
            redoLabel += " ";
            redoLabel += transactions.nextRedoLabel();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (!canUndo) ImGui::BeginDisabled();
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z"))
            {
                transactions.undo();
            }
            if (!canUndo) ImGui::EndDisabled();

            if (!canRedo) ImGui::BeginDisabled();
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y / Ctrl+Shift+Z"))
            {
                transactions.redo();
            }
            if (!canRedo) ImGui::EndDisabled();

            ImGui::EndMenu();
        }
    }

    if (ImGui::BeginMenu("Layout"))
    {
        if (ImGui::MenuItem("Reset To Default"))
        {
            m_requestLayoutReset = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        ImGui::MenuItem(kSceneWindowName, nullptr, &m_windowState.scene);
        ImGui::MenuItem(kHierarchyWindowName, nullptr, &m_windowState.hierarchy);
        ImGui::MenuItem(kInspectorWindowName, nullptr, &m_windowState.inspector);
        ImGui::MenuItem(kProjectWindowName, nullptr, &m_windowState.project);
        ImGui::MenuItem(kDebugHubWindowName, nullptr, &m_windowState.debugHub);
        ImGui::MenuItem(kSceneSettingsWindowName, nullptr, &m_windowState.sceneSettings);
        ImGui::EndMenu();
    }

    ImGui::SeparatorText("Workspace");
    ImGui::TextDisabled("Dock panels and toggle them from Window");

    ImGui::EndMenuBar();
}

void EditorManager::applyDefaultLayout(ImGuiID dockspaceId)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

    ImGuiID dockMain = dockspaceId;
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.26f, nullptr, &dockMain);
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.20f, nullptr, &dockMain);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);

    ImGui::DockBuilderDockWindow(kSceneWindowName, dockMain);
    ImGui::DockBuilderDockWindow(kHierarchyWindowName, dockLeft);
    ImGui::DockBuilderDockWindow(kInspectorWindowName, dockRight);
    ImGui::DockBuilderDockWindow(kProjectWindowName, dockBottom);
    ImGui::DockBuilderDockWindow(kDebugHubWindowName, dockBottom);
    ImGui::DockBuilderDockWindow(kSceneSettingsWindowName, dockRight);

    ImGui::DockBuilderFinish(dockspaceId);
}

void EditorManager::drawManagedWindows()
{
    if (m_windowState.sceneSettings)
    {
        SceneManager::Instance().debugOption();
    }

    if (m_windowState.scene)
    {
        DX12::Instance().sceneImguiRender();
        RuntimeUIManager::Instance().render();
    }

    if (m_windowState.project)
    {
        drawAssetBrowserWindow();
    }

    if (m_windowState.hierarchy)
    {
        drawHierarchyWindow();
    }

    if (m_windowState.inspector)
    {
        drawInspectorWindow();
    }

    if (m_windowState.debugHub)
    {
        drawDebugHubWindow();
    }
}

void EditorManager::drawDebugHubWindow()
{
    if (!ImGui::Begin("Debug Hub"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("DebugHubTabs"))
    {
        if (ImGui::BeginTabItem("Console"))
        {
            Logger::Instance().renderLogContents();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Profiler"))
        {
            TimeManager::Instance().renderProfilerContents();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Rendering"))
        {
            RenderManager::Instance().renderDebugContents();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Render Passes"))
        {
            RenderPipeline::Instance().renderDebugContents();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("GBuffer"))
        {
            GBufferRenderTargets::Instance().renderDebugContents();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Hi-Z"))
        {
            HiZPyramid::Instance().renderDebugContents();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio"))
        {
            AudioManager::Instance().renderDebugContents();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("NavMesh Bake"))
        {
            drawNavMeshBakeWindow();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("BT Graph"))
        {
            drawBehaviorTreeGraphEditorWindow();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Material Graph"))
        {
            drawMaterialGraphEditorWindow();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}