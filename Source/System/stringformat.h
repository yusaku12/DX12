#pragma once
#include <wtypes.h>

//! 文字列のフォーマッティング
template <typename... Args>
std::string stringFormat(const std::string& format, Args&&... args)
{
    // バッファサイズを計算
    int size_s = std::snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args) ...);
    if (size_s < 0) throw std::runtime_error("stringFormat error");

    size_t size = static_cast<size_t>(size_s) + 1; // 終端NULL分

    std::vector<char> buf(size);
    std::snprintf(buf.data(), size, format.c_str(), std::forward<Args>(args)...);
    return std::string(buf.data(), buf.data() + size - 1); // NULLを除く
}

//! wstringをstringへ変換
static std::string wstringToString(std::wstring oWString)
{
    // wstring → SJIS
    int iBufferSize = WideCharToMultiByte(CP_OEMCP, 0, oWString.c_str()
        , -1, (char*)NULL, 0, NULL, NULL);

    // バッファの取得
    std::vector<CHAR> cpMultiByte(static_cast<size_t>(iBufferSize));

    // wstring → SJIS
    WideCharToMultiByte(CP_OEMCP, 0, oWString.c_str(), -1, cpMultiByte.data()
        , iBufferSize, NULL, NULL);

    // stringの生成
    std::string oRet(cpMultiByte.data(), cpMultiByte.data() + iBufferSize - 1);

    // 変換結果を返す
    return(oRet);
}

//! string（SJIS）→ wstring
static std::wstring stringToWstring(const std::string& str)
{
    if (str.empty())
        return {};

    // 必要バッファサイズ取得
    int sizeNeeded = MultiByteToWideChar(
        CP_OEMCP,
        0,
        str.c_str(),
        -1,
        nullptr,
        0
    );

    if (sizeNeeded <= 0)
        throw std::runtime_error("stringToWstring conversion failed.");

    // バッファ確保
    std::vector<wchar_t> buffer(sizeNeeded);

    // 変換
    MultiByteToWideChar(
        CP_OEMCP,
        0,
        str.c_str(),
        -1,
        buffer.data(),
        sizeNeeded
    );

    // NULL終端を除いてwstring生成
    return std::wstring(buffer.data(), buffer.data() + sizeNeeded - 1);
}

//! 絶対パスを実行ファイルからの相対パスに変換するヘルパー
static std::wstring toRelativeWPath(const std::wstring& absolutePath)
{
    // wstring → filesystem::path
    std::filesystem::path absPath(absolutePath);

    // カレントディレクトリからの相対パスを算出
    std::error_code ec;
    std::filesystem::path relPath = std::filesystem::relative(absPath, std::filesystem::current_path(), ec);

    if (ec || relPath.empty())
    {
        // 相対化できなければそのまま返す
        return absolutePath;
    }

    return relPath.wstring();
}

//! 絶対パスを実行ファイルからの相対パスに変換するヘルパー
static std::string toRelativePath(const std::wstring& absolutePath)
{
    // wstring → filesystem::path
    std::filesystem::path absPath(absolutePath);

    // カレントディレクトリからの相対パスを算出
    std::error_code ec;
    auto relPath = std::filesystem::relative(absPath, std::filesystem::current_path(), ec);

    if (ec || relPath.empty())
    {
        // 相対化できなければ UTF-8 に変換してそのまま返す
        int sz = WideCharToMultiByte(CP_UTF8, 0, absolutePath.c_str(), static_cast<int>(absolutePath.size()), nullptr, 0, nullptr, nullptr);
        std::string result(sz, '\0');
        WideCharToMultiByte(CP_UTF8, 0, absolutePath.c_str(), static_cast<int>(absolutePath.size()), result.data(), sz, nullptr, nullptr);
        return result;
    }

    return relPath.string();
}

//! 絶対パスを実行ファイルからの相対パスに変換するヘルパー
static std::string toRelativePath(const char* absolutePath)
{
    // char* → filesystem::path
    std::filesystem::path absPath(absolutePath);

    // カレントディレクトリからの相対パスを算出
    std::error_code ec;
    auto relPath = std::filesystem::relative(absPath, std::filesystem::current_path(), ec);

    if (ec || relPath.empty())
    {
        // 相対化できなければそのまま返す
        return std::string(absolutePath);
    }

    return relPath.string();
}

//! UTF16 → UTF8
static bool uTF16ToUTF8(const wchar_t* utf16, char* utf8, int utf8_size)
{
    if (!utf16 || !utf8 || utf8_size <= 0)
        return false;

    int result = WideCharToMultiByte(
        CP_UTF8,            // UTF8
        0,
        utf16,
        -1,                 // NULL終端
        utf8,
        utf8_size,
        nullptr,
        nullptr);

    if (result == 0)
    {
        utf8[0] = '\0';
        return false;
    }

    return true;
}

//! ANSI文字列 → UTF8
static bool stringToUTF8(const char* string, char* utf8, int utf8_size)
{
    if (!string || !utf8 || utf8_size <= 0)
        return false;

    wchar_t utf16[1024];

    int utf16_len = MultiByteToWideChar(
        CP_ACP,
        0,
        string,
        -1,
        utf16,
        _countof(utf16));

    if (utf16_len == 0)
    {
        utf8[0] = '\0';
        return false;
    }

    return uTF16ToUTF8(utf16, utf8, utf8_size);
}