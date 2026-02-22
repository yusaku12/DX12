#pragma once

#include <shobjidl.h>

//! ダイアログの結果を表す列挙型
enum class DialogResult
{
    OK,
    Cancel,
    Yes,
    No
};

//------------------------------------------------------------------------------
//! ファイルダイアログを表示するためのクラス
//------------------------------------------------------------------------------
class Dialog
{
public:

    //! [ファイルを開く]ダイアログボックスを表示
    static DialogResult openFile(
        std::vector<std::wstring>& outPaths,
        const std::wstring& title = L"",
        const std::wstring& initialPath = L"",
        bool multiSelect = false);

    //! [ファイルを保存]ダイアログボックスを表示
    static DialogResult saveFile(
        std::wstring& outPath,
        const std::wstring& title = L"",
        const std::wstring& initialPath = L"",
        const std::wstring& defaultExtension = L"");

private:

    //! ダイアログの初期フォルダを設定するためのヘルパー関数
    static HRESULT setInitialFolder(
        IFileDialog* dialog,
        const std::wstring& path);
};