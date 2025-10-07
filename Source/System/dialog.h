#pragma once
#include <string>
#include <wtypes.h>

//! �_�C�A���O���U���g
enum class DialogResult
{
    Yes,
    No,
    OK,
    Cancel
};

//=====================================================
// Dialog �N���X
//=====================================================
class Dialog
{
public:
    //! [�t�@�C�����J��]�_�C�A���O�{�b�N�X��\��
    static DialogResult openFileName(char* filepath, int size, const char* filter = nullptr, const char* title = nullptr, HWND hWnd = NULL, const std::string& initialPath = {}, bool multiSelect = false);

    //! [�t�@�C����ۑ�]�_�C�A���O�{�b�N�X��\��
    static DialogResult saveFileName(char* filepath, int size, const char* filter = nullptr, const char* title = nullptr, const char* ext = nullptr, HWND hWnd = NULL, const std::string& initialPath = {});
};