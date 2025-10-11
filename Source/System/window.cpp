#include "pch.h"
#include "window.h"

Window::Window(HWND hwnd)
    : m_hwnd(hwnd)
    , m_dx12(hwnd)
{
    //! imgui������
    IMGUI_CTRL_INITIALIZE();

    //! �|���S���N���X������
    m_polygon = std::make_unique<Polygon>();
}

Window::~Window()
{
    //! GPU �����҂�
    m_dx12.safeGPUWait();

    //! ImGui ������ꍇ�͐�ɏI������
    IMGUI_CTRL_FINALIZE();
}

void Window::update()
{
    //! imgui�X�V
    IMGUI_CTRL_UPDATE();
}

void Window::render()
{
    //! ���O�`��
    Logger::getInstance().renderLog();

    //! imgui�`��
    IMGUI_CTRL_RENDER();

    //! ��ʂ��N���A
    m_dx12.screenClear();

    //! �|���S���`��
    m_polygon->render();

    //! imgui�̕`�����ݒ�
    IMGUI_CTRL_RENDER_INFO();

    //! �����_�[�^�[�Q�b�g�����ɖ߂��A�R�}���h�I��
    m_dx12.renderTargetUndo();

    //! �v���b�g�t�H�[����ǉ����ăE�B���h�E�X�V
    IMGUI_CTRL_UPDATE_RENDER();

    //! ��ʃN���A��̌㏈��
    m_dx12.screenClearCleanup();
}

int Window::run()
{
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            //! �X�V�A�`��
            update();
            render();
        }
    }
    return static_cast<int>(msg.wParam);
}

LRESULT Window::processMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    //! ImGui �Ƀ��b�Z�[�W��n��
    IMGUI_CTRL_WND_PRC_HANDLER(hwnd, msg, wparam, lparam);

    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED)
        {
            Logger::getInstance().logCall(LogLevel::INFO, "resize");
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);
            m_dx12.screenResize(width, height);
            IMGUI_CTRL_RESIZE(width, height);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    return 0;
}