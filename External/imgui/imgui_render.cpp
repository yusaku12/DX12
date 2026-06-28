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
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.Fonts->AddFontFromFileTTF("Data\\Font\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(8, 8);
        style.FramePadding = ImVec2(8, 6);
        style.ItemSpacing = ImVec2(8, 6);
        style.ItemInnerSpacing = ImVec2(6, 4);
        style.IndentSpacing = 16;
        style.ScrollbarSize = 15;
        style.GrabMinSize = 11;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 0;
        style.TabBorderSize = 0;

        style.WindowRounding = 6.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 5.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 5.0f;
        style.TabRounding = 5.0f;

        ImVec4* colors = style.Colors;
        const float alpha = 1.0f;

        colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.145f, 0.16f, alpha);
        colors[ImGuiCol_ChildBg] = ImVec4(0.115f, 0.12f, 0.135f, alpha);
        colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.125f, 0.14f, 0.98f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.105f, 0.115f, alpha);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.09f, 0.095f, 0.105f, alpha);

        colors[ImGuiCol_FrameBg] = ImVec4(0.19f, 0.205f, 0.23f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.275f, 0.33f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.27f, 0.31f, 0.37f, 1.0f);

        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.235f, 0.285f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.27f, 0.34f, 0.44f, 1.0f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.26f, 0.36f, 1.0f);

        colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.125f, 0.14f, alpha);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.18f, 0.22f, alpha);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.11f, 0.115f, 0.125f, alpha);

        colors[ImGuiCol_Header] = ImVec4(0.18f, 0.205f, 0.25f, 1.0f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.31f, 0.40f, 1.0f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.19f, 0.27f, 0.37f, 1.0f);

        colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.16f, 0.18f, alpha);
        colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.33f, 0.43f, alpha);
        colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.27f, 0.37f, alpha);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.13f, 0.145f, alpha);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.19f, 0.24f, alpha);

        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.085f, 0.095f, alpha);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.28f, 0.34f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.36f, 0.45f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.41f, 0.50f, 1.0f);

        colors[ImGuiCol_Border] = ImVec4(0.24f, 0.27f, 0.31f, 0.65f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        colors[ImGuiCol_CheckMark] = ImVec4(0.86f, 0.91f, 0.98f, 1.0f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.42f, 0.54f, 1.0f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.43f, 0.54f, 0.70f, 1.0f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.24f, 0.40f, 0.62f, 0.45f);

        ImGui_ImplWin32_Init(DX12::Instance().getHwnd());
        ImGui_ImplDX12_InitInfo init_info = {};
        init_info.Device = DX12::Instance().getDevice();
        init_info.CommandQueue = DX12::Instance().getCommandQueue();
        init_info.NumFramesInFlight = 3;
        init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
        init_info.SrvDescriptorHeap = DescriptorHeapManager::Instance().getHeap();
        init_info.UserData = &DX12::Instance();

        // ImGui_ImplDX12 が期待するシグネチャ:
        init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle) -> void
            {
                auto dx12Imgui = reinterpret_cast<DX12*>(info->UserData);
                auto allocation = dx12Imgui->getExampleDescriptorHeapAllocator().Allocate();
                if (out_cpu_desc_handle) *out_cpu_desc_handle = allocation.cpuHandle;
                if (out_gpu_desc_handle) *out_gpu_desc_handle = allocation.gpuHandle;
            };

        init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE /*gpu_desc_handle*/) -> void
            {
                auto dx12Imgui = reinterpret_cast<DX12*>(info->UserData);
                auto& alloc = dx12Imgui->getExampleDescriptorHeapAllocator();
                // CPUハンドルからインデックスを復元して解放
                UINT index = 0;
                if (alloc.HeapHandleIncrement != 0)
                {
                    index = static_cast<UINT>((cpu_desc_handle.ptr - alloc.HeapStartCpu.ptr) / alloc.HeapHandleIncrement);
                }
                alloc.FreeByIndex(index);
            };

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
    }

    void render()
    {
        //! DescriptorHeap
        DescriptorHeapManager::Instance().setDescriptorHeap();

        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), DX12::Instance().getGraphicsCommandList());
    }

    void resize(int width, int height)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    }
}