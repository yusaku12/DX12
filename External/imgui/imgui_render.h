#pragma once

#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace ImGuiCtrl
{
    //! ������
    void initialize();

    //! �I������
    void finalize();

    //! �X�V
    void update();

    //! �`��
    void render();

    //! �`������Z�b�g
    void renderInfo();

    //! �v���b�g�t�H�[����ǉ����ăE�B���h�E�X�V
    void updateRender();

    //! ���T�C�Y
    void resize(int width, int height);
}

//! imgui�g�p���邽�߂̃}�N��
#define IMGUI_CTRL_WND_PRC_HANDLER(hwnd, msg, wParam, lParam) \
if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true
#define IMGUI_CTRL_INITIALIZE() ImGuiCtrl::initialize()
#define IMGUI_CTRL_UPDATE() ImGuiCtrl::update()
#define IMGUI_CTRL_FINALIZE() ImGuiCtrl::finalize()
#define IMGUI_CTRL_RESIZE(width,height) ImGuiCtrl::resize(width,height)
#define IMGUI_CTRL_RENDER() ImGuiCtrl::render()
#define IMGUI_CTRL_RENDER_INFO() ImGuiCtrl::renderInfo()
#define IMGUI_CTRL_UPDATE_RENDER() ImGuiCtrl::updateRender()