#pragma once

//! 文字列のフォーマッティング
template <typename... Args>
std::string stringFormat(const std::string& format, Args&&... args)
{
    // バッファサイズを計算
    int size_s = std::snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args) ...);
    if (size_s < 0) throw std::runtime_error("stringFormat error");

    size_t size = static_cast<size_t>(size_s) + 1; //!< 終端NULL分

    std::vector<char> buf(size);
    std::snprintf(buf.data(), size, format.c_str(), std::forward<Args>(args)...);
    return std::string(buf.data(), buf.data() + size - 1); //!< NULLを除く
}

//! wstringをstringへ変換
static std::string wstringToString(std::wstring oWString)
{
    //! wstring → SJIS
    int iBufferSize = WideCharToMultiByte(CP_OEMCP, 0, oWString.c_str()
        , -1, (char*)NULL, 0, NULL, NULL);

    //! バッファの取得
    CHAR* cpMultiByte = new CHAR[iBufferSize];

    //! wstring → SJIS
    WideCharToMultiByte(CP_OEMCP, 0, oWString.c_str(), -1, cpMultiByte
        , iBufferSize, NULL, NULL);

    //! stringの生成
    std::string oRet(cpMultiByte, cpMultiByte + iBufferSize - 1);

    //! バッファの破棄
    delete[] cpMultiByte;

    //! 変換結果を返す
    return(oRet);
}

//! string（SJIS）→ wstring
static std::wstring stringToWstring(const std::string& str)
{
    if (str.empty())
        return {};

    //! 必要バッファサイズ取得
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

    //! バッファ確保
    std::vector<wchar_t> buffer(sizeNeeded);

    //! 変換
    MultiByteToWideChar(
        CP_OEMCP,
        0,
        str.c_str(),
        -1,
        buffer.data(),
        sizeNeeded
    );

    //! NULL終端を除いてwstring生成
    return std::wstring(buffer.data(), buffer.data() + sizeNeeded - 1);
}