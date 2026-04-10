#pragma once

#include "FxDefines.hpp"

#include <format>
#include <fstream>
#include <iostream>
#include <string>

enum class FxLogChannel
{
    None,
    Debug,
    Info,
    Warning,
    Error,
    Fatal,
};


#define FX_LOG_CHANNEL_LABEL_DEBUG "[DEBUG] "
#define FX_LOG_CHANNEL_LABEL_INFO  "[INFO]  "
#define FX_LOG_CHANNEL_LABEL_WARN  "[WARN]  "
#define FX_LOG_CHANNEL_LABEL_ERROR "[ERROR] "
#define FX_LOG_CHANNEL_LABEL_FATAL "[FATAL] "

#define FX_LOG_STYLE_RESET "\x1b[0m"

std::ofstream& FxLogGetFile(bool* can_write);
void FxLogCreateFile(const std::string& path);

template <FxLogChannel TLogChannel, bool TAllowColors = true>
constexpr std::string FxLogGetChannelText()
{
    if constexpr (TLogChannel == FxLogChannel::None) {
        return "";
    }

#ifdef FX_LOG_ENABLE_COLORS
    constexpr bool AllowColors = TAllowColors;
#else
    // Colours are disabled globally on compilation, never show them
    constexpr bool AllowColors = false;
#endif

    if (AllowColors) {
        if constexpr (TLogChannel == FxLogChannel::Debug) {
            return ("\x1b[92m" FX_LOG_CHANNEL_LABEL_DEBUG FX_LOG_STYLE_RESET);
        }
        else if constexpr (TLogChannel == FxLogChannel::Info) {
            return ("\x1b[94m" FX_LOG_CHANNEL_LABEL_INFO FX_LOG_STYLE_RESET);
        }
        else if constexpr (TLogChannel == FxLogChannel::Warning) {
            return ("\x1b[93m" FX_LOG_CHANNEL_LABEL_WARN FX_LOG_STYLE_RESET);
        }
        else if constexpr (TLogChannel == FxLogChannel::Error) {
            return ("\x1b[91m" FX_LOG_CHANNEL_LABEL_ERROR FX_LOG_STYLE_RESET);
        }
        else if constexpr (TLogChannel == FxLogChannel::Fatal) {
            return ("\x1b[1;91m" FX_LOG_CHANNEL_LABEL_FATAL FX_LOG_STYLE_RESET);
        }
    }
    else {
        if constexpr (TLogChannel == FxLogChannel::Debug) {
            return FX_LOG_CHANNEL_LABEL_DEBUG;
        }
        else if constexpr (TLogChannel == FxLogChannel::Info) {
            return FX_LOG_CHANNEL_LABEL_INFO;
        }
        else if constexpr (TLogChannel == FxLogChannel::Warning) {
            return FX_LOG_CHANNEL_LABEL_WARN;
        }
        else if constexpr (TLogChannel == FxLogChannel::Error) {
            return FX_LOG_CHANNEL_LABEL_ERROR;
        }
        else if constexpr (TLogChannel == FxLogChannel::Fatal) {
            return FX_LOG_CHANNEL_LABEL_FATAL;
        }
    }

    return "";
}


template <typename... TTypes>
void FxLogDirectToStdout(std::string_view fmt, TTypes&&... args)
{
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    std::cout << msg;
}

template <FxLogChannel TChannel, typename... TTypes>
void FxLogToStdout(std::string_view fmt, TTypes&&... args)
{
    // Disregard debug logs when building for release
#ifdef FX_BUILD_RELEASE
    if constexpr (TChannel == FxLogChannel::Debug) {
        return;
    }
#endif

    auto msg = std::vformat(fmt, std::make_format_args(args...));

    // If the channel is set to `None`, do not print the channel name
    if constexpr (TChannel == FxLogChannel::None) {
        std::cout << msg << '\n';
        return;
    }

    auto channel = FxLogGetChannelText<TChannel>();

    std::cout << channel << msg << '\n';
}


template <typename... TTypes>
void FxLogDirectToFile(std::string_view fmt, TTypes&&... args)
{
    bool can_write = false;
    std::ofstream& stream = FxLogGetFile(&can_write);

    if (!can_write) {
        return;
    }

    auto msg = std::vformat(fmt, std::make_format_args(args...));

    // If the channel is set to `None`, do not print the channel name
    stream << msg << '\n';
}


template <FxLogChannel TChannel, typename... TTypes>
void FxLogToFile(std::string_view fmt, TTypes&&... args)
{
    // Disregard debug logs when building for release
#ifdef FX_BUILD_RELEASE
    if constexpr (TChannel == FxLogChannel::Debug) {
        return;
    }
#endif

    bool can_write = false;
    std::ofstream& stream = FxLogGetFile(&can_write);

    if (!can_write) {
        return;
    }

    auto msg = std::vformat(fmt, std::make_format_args(args...));

    // If the channel is set to `None`, do not print the channel name
    if constexpr (TChannel == FxLogChannel::None) {
        stream << msg << '\n';
        return;
    }

    auto channel = FxLogGetChannelText<TChannel, false>();

    stream << channel << msg << '\n';
}

/**
 * @brief Logs a message to a channel from `FxLogChannel`.
 */
template <FxLogChannel TChannel, typename... TTypes>
void FxLog(std::string_view fmt, TTypes&&... args)
{
    // Disregard debug logs when building for release
#ifdef FX_BUILD_RELEASE
    if constexpr (TChannel == FxLogChannel::Debug) {
        return;
    }
#endif

#ifdef FX_LOG_OUTPUT_TO_STDOUT
    FxLogToStdout<TChannel>(fmt, std::forward<TTypes>(args)...);
#endif

#ifdef FX_LOG_OUTPUT_TO_FILE
    FxLogToFile<TChannel>(fmt, std::forward<TTypes>(args)...);
#endif
}

/**
 * @brief Logs a message to a channel from `FxLogChannel`.
 */
template <typename... TTypes>
void FxLogDirect(std::string_view fmt, TTypes&&... args)
{
#ifdef FX_LOG_OUTPUT_TO_STDOUT
    FxLogDirectToStdout(fmt, std::forward<TTypes>(args)...);
#endif

#ifdef FX_LOG_OUTPUT_TO_FILE
    FxLogDirectToFile(fmt, std::forward<TTypes>(args)...);
#endif
}

template <typename... TTypes>
void FxLogInfo(std::string_view fmt, TTypes&&... args)
{
    FxLog<FxLogChannel::Info>(fmt, std::forward<TTypes>(args)...);
}

template <typename... TTypes>
void FxLogDebug(std::string_view fmt, TTypes&&... args)
{
    FxLog<FxLogChannel::Debug>(fmt, std::forward<TTypes>(args)...);
}

template <typename... TTypes>
void FxLogWarning(std::string_view fmt, TTypes&&... args)
{
    FxLog<FxLogChannel::Warning>(fmt, std::forward<TTypes>(args)...);
}

template <typename... TTypes>
void FxLogError(std::string_view fmt, TTypes&&... args)
{
    FxLog<FxLogChannel::Error>(fmt, std::forward<TTypes>(args)...);
}

template <typename... TTypes>
void FxLogFatal(std::string_view fmt, TTypes&&... args)
{
    FxLog<FxLogChannel::Fatal>(fmt, std::forward<TTypes>(args)...);
}


template <FxLogChannel TChannel>
constexpr void FxLogChannelText()
{
#ifdef FX_LOG_OUTPUT_TO_STDOUT
    FxLogDirectToStdout("{}", FxLogGetChannelText<TChannel>());
#endif
#ifdef FX_LOG_OUTPUT_TO_FILE
    FxLogDirectToFile("{}", FxLogGetChannelText<TChannel, false>());
#endif
}
