/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Logger_Log(LogChannel* channel, uint32 logLevel, const char* funcName, uint32 line, const char* message)
    {
        if (!channel)
        {
            channel = &g_logChannel_Script;
        }

        if (logLevel > uint32(LogLevel::FATAL))
        {
            logLevel = uint32(LogLevel::FATAL);
        }

        switch (LogLevel(logLevel))
        {
        case LogLevel::DEBUG:
            logging::LogStatic_Channel<logging::Debug(), StaticString("{}\n")>(logging::GetLogger(), *channel, message);

            break;
        case LogLevel::INFO:
            logging::LogStatic_Channel<logging::Info(), StaticString("{}\n")>(logging::GetLogger(), *channel, message);

            break;
        case LogLevel::WARNING:
            logging::LogStatic_Channel<logging::Warning(), StaticString("{}\n")>(logging::GetLogger(), *channel, message);

            break;
        case LogLevel::ERR:
            logging::LogStatic_Channel<logging::Error(), StaticString("{}\n")>(logging::GetLogger(), *channel, message);

            break;
        case LogLevel::FATAL:
            logging::LogStatic_Channel<logging::Fatal(), StaticString("{}\n")>(logging::GetLogger(), *channel, message);

            break;
        }
    }

    HYP_EXPORT const LogChannel* Logger_FindLogChannel(WeakName* name)
    {
        if (!name)
        {
            return nullptr;
        }

        return Logger::GetInstance().FindLogChannel(*name);
    }

    HYP_EXPORT LogChannel* Logger_CreateLogChannel(const char* name)
    {
        const Name channelName = CreateNameFromDynamicString(name);

        // owns allocation
        LogChannel* logChannel = new LogChannel(channelName, &g_logChannel_Script);

        Logger::GetInstance()
            .CreateDynamicLogChannel(*logChannel)
            .Release();

        return logChannel;
    }

    HYP_EXPORT void Logger_DestroyLogChannel(LogChannel* logChannel)
    {
        if (!logChannel)
        {
            return;
        }

        Logger::GetInstance().RemoveDynamicLogChannel(logChannel);

        // owns allocation
        delete logChannel;
    }

} // extern "C"
