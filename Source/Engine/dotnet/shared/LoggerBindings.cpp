/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void Logger_Log(LogChannel* pChannel, uint32 logLevel, const char* pFileName, uint32 line, const char* pMessage)
    {
        if (!pChannel)
        {
            pChannel = &g_logChannel_Script;
        }

        switch (LogLevel(logLevel))
        {
        case LogLevel::Debug:
            logging::LogDynamic<LogLevel::Debug>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        case LogLevel::Verbose:
            logging::LogDynamic<LogLevel::Verbose>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        case LogLevel::Info:
            logging::LogDynamic<LogLevel::Info>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        case LogLevel::Warning:
            logging::LogDynamic<LogLevel::Warning>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        case LogLevel::Error:
            logging::LogDynamic<LogLevel::Error>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        case LogLevel::Fatal:
            logging::LogDynamic<LogLevel::Fatal>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        default:
            HYP_UNREACHABLE();
        }
    }

    HYP_EXPORT const LogChannel* Logger_FindLogChannel(StringHash* pNameHash)
    {
        if (!pNameHash)
        {
            return nullptr;
        }

        return Logger::GetInstance().FindLogChannel(*pNameHash);
    }

    HYP_EXPORT LogChannel* Logger_CreateLogChannel(const char* pName)
    {
        const Name channelName = CreateNameFromDynamicString(pName);

        // owns allocation
        LogChannel* pChannel = new LogChannel(channelName, &g_logChannel_Script);

        Logger::GetInstance().CreateDynamicLogChannel(*pChannel).Release();

        return pChannel;
    }

    HYP_EXPORT void Logger_DestroyLogChannel(LogChannel* pChannel)
    {
        if (!pChannel)
        {
            return;
        }

        Logger::GetInstance().RemoveDynamicLogChannel(pChannel);

        // owns allocation
        delete pChannel;
    }

} // extern "C"
