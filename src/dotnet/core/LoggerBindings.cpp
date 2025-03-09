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
        logging::Log_Internal< logging::Debug(), StaticString("<script>"), StaticString("{}:{}: {}\n") >(logging::GetLogger(), *channel, func_name, line, message);

        break;
    case LogLevel::INFO:
        logging::Log_Internal< logging::Info(), StaticString("<script>"), StaticString("{}:{}: {}\n") >(logging::GetLogger(), *channel, func_name, line, message);

        break;
    case LogLevel::WARNING:
        logging::Log_Internal< logging::Warning(), StaticString("<script>"), StaticString("{}:{}: {}\n") >(logging::GetLogger(), *channel, func_name, line, message);

        break;
    case LogLevel::ERR:
        logging::Log_Internal< logging::Error(), StaticString("<script>"), StaticString("{}:{}: {}\n") >(logging::GetLogger(), *channel, func_name, line, message);

        break;
    case LogLevel::FATAL:
        logging::Log_Internal< logging::Fatal(), StaticString("<script>"), StaticString("{}:{}: {}") >(logging::GetLogger(), *channel, func_name, line, message);

        break;
    }
}

HYP_EXPORT const LogChannel *Logger_FindLogChannel(WeakName *name)
{
    if (!name) {
        return nullptr;
    }

    return Logger::GetInstance().FindLogChannel(*name);
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