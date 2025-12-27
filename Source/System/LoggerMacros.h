#pragma once

//=====================================================
// 基本ログ
//=====================================================
#define LOG_INFO(fmt, ...)  Logger::Instance().logCall(LogLevel::INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::Instance().logCall(LogLevel::WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::Instance().logCall(LogLevel::ERROR, fmt, ##__VA_ARGS__)

//=====================================================
// ファイル・行・関数付き
//=====================================================
#define LOG_INFO_EX(fmt, ...)  \
    Logger::Instance().logCall(LogLevel::INFO,  "[%s:%d %s] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define LOG_WARN_EX(fmt, ...)  \
    Logger::Instance().logCall(LogLevel::WARN,  "[%s:%d %s] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define LOG_ERROR_EX(fmt, ...) \
    Logger::Instance().logCall(LogLevel::ERROR, "[%s:%d %s] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

//=====================================================
// ASSERT（デバッグのみ有効）
//=====================================================
#ifdef _DEBUG

#define LOG_ASSERT(expr, fmt, ...)                                      \
    do                                                                  \
    {                                                                   \
        if (!(expr))                                                    \
        {                                                               \
            Logger::Instance().logCall(                                 \
                LogLevel::ERROR,                                        \
                "[ASSERT FAILED] %s (%s:%d)\n" fmt,                     \
                #expr, __FILE__, __LINE__, ##__VA_ARGS__);              \
            __debugbreak();                                             \
        }                                                               \
    } while (0)

#define LOG_ASSERT_NO_JUDGE(fmt, ...)                                   \
    do                                                                  \
    {                                                                   \
        Logger::Instance().logCall(                                     \
            LogLevel::ERROR,                                            \
            "[ASSERT FAILED] " fmt " (%s:%d)\n",                        \
            ##__VA_ARGS__, __FILE__, __LINE__);                         \
        __debugbreak();                                                 \
    } while (0)

#else
#define LOG_ASSERT(expr, fmt, ...) ((void)0)
#endif

//=====================================================
// HRESULT チェック（DX12用）
//=====================================================
#define LOG_HR(hr, fmt, ...)                                            \
    do                                                                  \
    {                                                                   \
        if (FAILED(hr))                                                 \
        {                                                               \
            Logger::Instance().logCall(                                 \
                LogLevel::ERROR,                                        \
                "[HRESULT FAILED] 0x%08X (%s:%d)\n" fmt,                \
                hr, __FILE__, __LINE__, ##__VA_ARGS__);                 \
        }                                                               \
    } while (0)