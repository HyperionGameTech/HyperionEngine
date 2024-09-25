/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void Logger_Log(LogChannel *channel, uint32 log_level, const char *func_name, uint32 line, const char *message)
{
    if (!channel) {
        channel = &Log_Script;
    }

    if (log_level > uint32(LogLevel::FATAL)) {
        log_level = uint32(LogLevel::FATAL);
    }

    switch (LogLevel(log_level)) {
    case LogLevel::DEBUG:
        Logger::GetInstance().template Log< LogLevel::DEBUG, StaticString("<script>"), StaticString("{}:{}: {}") >(*channel, func_name, line, message);

        break;
    case LogLevel::INFO:
        Logger::GetInstance().template Log< LogLevel::INFO, StaticString("<script>"), StaticString("{}:{}: {}") >(*channel, func_name, line, message);

        break;
    case LogLevel::WARNING:
        Logger::GetInstance().template Log< LogLevel::WARNING, StaticString("<script>"), StaticString("{}:{}: {}") >(*channel, func_name, line, message);

        break;
    case LogLevel::ERR:
        Logger::GetInstance().template Log< LogLevel::ERR, StaticString("<script>"), StaticString("{}:{}: {}") >(*channel, func_name, line, message);

        break;
    case LogLevel::FATAL:
        Logger::GetInstance().template Log< LogLevel::FATAL, StaticString("<script>"), StaticString("{}:{}: {}") >(*channel, func_name, line, message);

        break;
    }
}

HYP_EXPORT LogChannel *Logger_CreateLogChannel(const char *name)
{
    const Name channel_name = CreateNameFromDynamicString(name);

    return Logger::GetInstance().CreateDynamicLogChannel(channel_name, &Log_Script);
}

HYP_EXPORT void Logger_DestroyLogChannel(LogChannel *log_channel)
{
    if (!log_channel) {
        return;
    }

    Logger::GetInstance().DestroyDynamicLogChannel(log_channel);
}

} // extern "C"