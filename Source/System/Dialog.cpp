#include "pch.h"
#include "Dialog.h"
#include <shlwapi.h>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")

HRESULT Dialog::setInitialFolder(
    IFileDialog* dialog,
    const std::wstring& path)
{
    if (path.empty())
        return S_OK;

    IShellItem* folder = nullptr;

    HRESULT hr = SHCreateItemFromParsingName(
        path.c_str(),
        nullptr,
        IID_PPV_ARGS(&folder));

    if (SUCCEEDED(hr))
    {
        dialog->SetFolder(folder);
        folder->Release();
    }

    return hr;
}

DialogResult Dialog::openFile(
    std::vector<std::wstring>& outPaths,
    const std::wstring& title,
    const std::wstring& initialPath,
    bool multiSelect)
{
    outPaths.clear();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitialized = SUCCEEDED(hr);

    IFileOpenDialog* dialog = nullptr;

    hr = CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&dialog));

    if (FAILED(hr))
        return DialogResult::Cancel;

    DWORD options;
    dialog->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM;

    if (multiSelect)
        options |= FOS_ALLOWMULTISELECT;

    dialog->SetOptions(options);

    if (!title.empty())
        dialog->SetTitle(title.c_str());

    setInitialFolder(dialog, initialPath);

    hr = dialog->Show(nullptr);

    if (FAILED(hr))
    {
        dialog->Release();
        if (comInitialized) CoUninitialize();
        return DialogResult::Cancel;
    }

    if (multiSelect)
    {
        IShellItemArray* items = nullptr;
        dialog->GetResults(&items);

        DWORD count = 0;
        items->GetCount(&count);

        for (DWORD i = 0; i < count; ++i)
        {
            IShellItem* item = nullptr;
            items->GetItemAt(i, &item);

            PWSTR path = nullptr;
            item->GetDisplayName(SIGDN_FILESYSPATH, &path);

            outPaths.emplace_back(path);

            CoTaskMemFree(path);
            item->Release();
        }

        items->Release();
    }
    else
    {
        IShellItem* item = nullptr;
        dialog->GetResult(&item);

        PWSTR path = nullptr;
        item->GetDisplayName(SIGDN_FILESYSPATH, &path);

        outPaths.emplace_back(path);

        CoTaskMemFree(path);
        item->Release();
    }

    dialog->Release();

    if (comInitialized)
        CoUninitialize();

    return DialogResult::OK;
}

DialogResult Dialog::saveFile(
    std::wstring& outPath,
    const std::wstring& title,
    const std::wstring& initialPath,
    const std::wstring& defaultExtension)
{
    outPath.clear();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitialized = SUCCEEDED(hr);

    IFileSaveDialog* dialog = nullptr;

    hr = CoCreateInstance(
        CLSID_FileSaveDialog,
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&dialog));

    if (FAILED(hr))
        return DialogResult::Cancel;

    DWORD options;
    dialog->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT;
    dialog->SetOptions(options);

    if (!title.empty())
        dialog->SetTitle(title.c_str());

    if (!defaultExtension.empty())
        dialog->SetDefaultExtension(defaultExtension.c_str());

    setInitialFolder(dialog, initialPath);

    hr = dialog->Show(nullptr);

    if (FAILED(hr))
    {
        dialog->Release();
        if (comInitialized) CoUninitialize();
        return DialogResult::Cancel;
    }

    IShellItem* item = nullptr;
    dialog->GetResult(&item);

    PWSTR path = nullptr;
    item->GetDisplayName(SIGDN_FILESYSPATH, &path);

    outPath = path;

    CoTaskMemFree(path);
    item->Release();
    dialog->Release();

    if (comInitialized)
        CoUninitialize();

    return DialogResult::OK;
}