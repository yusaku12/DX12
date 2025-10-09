#include "pch.h"

namespace ImGuiCtrl
{
    void initialize()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
        io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;     // FIXME-DPI: THIS CURRENTLY DOESN'T WORK AS EXPECTED. DON'T USE IN USER APP!
        io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports; // FIXME-DPI
        io.Fonts->AddFontFromFileTTF("Data\\Font\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(6, 6);
        style.FramePadding = ImVec2(5, 3);
        style.ItemSpacing = ImVec2(6, 4);
        style.ItemInnerSpacing = ImVec2(4, 4);
        style.IndentSpacing = 12;
        style.ScrollbarSize = 14;
        style.GrabMinSize = 10;

        style.WindowBorderSize = 1;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 0;
        style.TabBorderSize = 0;

        style.WindowRounding = 3;
        style.ChildRounding = 3;
        style.FrameRounding = 2;
        style.ScrollbarRounding = 2;
        style.GrabRounding = 2;
        style.TabRounding = 2;

        ImVec4* colors = style.Colors;
        const float alpha = 1.0f;

        // 基本背景
        colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, alpha);
        colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.16f, 0.16f, alpha);
        colors[ImGuiCol_PopupBg] = ImVec4(0.16f, 0.16f, 0.16f, alpha);

        // フレーム（ボタン、スライダー等）
        colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);

        // ボタン
        colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);

        // タイトルバー
        colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.14f, alpha);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.20f, 0.20f, alpha);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, alpha);

        // ヘッダー（ツリーノード、折りたたみなど）
        colors[ImGuiCol_Header] = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);

        // タブ
        colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, alpha);
        colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.28f, 0.28f, alpha);
        colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.22f, 0.22f, alpha);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.15f, 0.15f, alpha);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.18f, alpha);

        // スクロールバー
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.14f, 0.14f, 0.14f, alpha);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

        // 境界線
        colors[ImGuiCol_Border] = ImVec4(0.10f, 0.10f, 0.10f, alpha);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        // 選択
        colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.33f, 0.33f, 0.33f, 1.0f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 0.50f, 0.90f, 0.35f);

        // ビューポート対応
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        ImGui_ImplWin32_Init(DX12::getInstance().getHwnd());
        ImGui_ImplDX12_InitInfo init_info = {};
        init_info.Device = DX12::getInstance().getDevice();
        init_info.CommandQueue = DX12::getInstance().getCommandQueue();
        init_info.NumFramesInFlight = 3;
        init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
        init_info.SrvDescriptorHeap = DX12::getInstance().getSRVDiscriptorHeap();
        init_info.UserData = &DX12::getInstance();
        init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) {auto dx12 = reinterpret_cast<DX12*>(info->UserData); return dx12->getExampleDescriptorHeapAllocator().Alloc(out_cpu_handle, out_gpu_handle); };
        init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {auto dx12 = reinterpret_cast<DX12*>(info->UserData); return dx12->getExampleDescriptorHeapAllocator().Free(cpu_handle, gpu_handle); };
        ImGui_ImplDX12_Init(&init_info);
    }

    void finalize()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void update()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
        ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, nullptr);
    }

    void render()
    {
        ImGui::Render();
    }

    void renderInfo()
    {
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), DX12::getInstance().getGraphicsCommandList());
    }

    void updateRender()
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void resize(int width, int height)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    }
}