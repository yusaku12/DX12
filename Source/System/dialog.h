#pragma once
#include <string>
#include <wtypes.h>

//! ダイアログリザルト
enum class DialogResult
{
    Yes,
    No,
    OK,
    Cancel
};

//=====================================================
// Dialog クラス
//=====================================================
class Dialog
{
public:
    //! [ファイルを開く]ダイアログボックスを表示
    static DialogResult openFileName(char* filepath, int size, const char* filter = nullptr, const char* title = nullptr, HWND hWnd = NULL, const std::string& initialPath = {}, bool multiSelect = false);

    //! [ファイルを保存]ダイアログボックスを表示
    static DialogResult saveFileName(char* filepath, int size, const char* filter = nullptr, const char* title = nullptr, const char* ext = nullptr, HWND hWnd = NULL, const std::string& initialPath = {});
};