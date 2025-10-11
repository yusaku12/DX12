#pragma once

#include "Graphics\polygon.h"

//=====================================================
// Window �N���X
//=====================================================
class Window
{
public:

    Window(HWND hwnd);
    ~Window();

    //! �X�V����
    void update();

    //! �`�揈��
    void render();

    //! ���b�Z�[�W���[�v
    int run();

    //! �E�B���h�E�v���V�[�W��
    LRESULT processMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:

    const HWND m_hwnd;
    DX12 m_dx12;
    std::unique_ptr<Polygon> m_polygon;
};