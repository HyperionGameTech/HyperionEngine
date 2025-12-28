/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

using namespace Hyperion;

namespace Hyperion {
HYP_API extern Handle<Logger> g_logger;
} // namespace Hyperion

extern "C"
{

    HYP_EXPORT void Logger_Log(LogChannel* pChannel, uint32 logLevel, const char* pFileName, uint32 line, const char* pMessage)
    {
        if (!pChannel)
        {
            pChannel = &g_logChannel_Script;
        }

        if (logLevel > uint32(LogLevel::FATAL))
        {
            logLevel = uint32(LogLevel::FATAL);
        }

        switch (LogLevel(logLevel))
        {
        case LogLevel::DEBUG:
            logging::LogDynamic<HYP_MAKE_CONST_ARG(&logging::Debug())>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        case LogLevel::INFO:
            logging::LogDynamic<HYP_MAKE_CONST_ARG(&logging::Info())>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        case LogLevel::WARNING:
            logging::LogDynamic<HYP_MAKE_CONST_ARG(&logging::Warning())>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        case LogLevel::ERR:
            logging::LogDynamic<HYP_MAKE_CONST_ARG(&logging::Error())>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        case LogLevel::FATAL:
            logging::LogDynamic<HYP_MAKE_CONST_ARG(&logging::Fatal())>(logging::GetLogger(), *pChannel, pFileName, line, pMessage);

            break;
        }
    }

    HYP_EXPORT const LogChannel* Logger_FindLogChannel(StringHash* pNameHash)
    {
        if (!pNameHash)
        {
            return nullptr;
        }

        return g_logger->FindLogChannel(*pNameHash);
    }

    HYP_EXPORT LogChannel* Logger_CreateLogChannel(const char* pName)
    {
        const Name channelName = CreateNameFromDynamicString(pName);

        // owns allocation
        LogChannel* pChannel = new LogChannel(channelName, &g_logChannel_Script);

        g_logger->CreateDynamicLogChannel(*pChannel).Release();

        return pChannel;
    }

    HYP_EXPORT void Logger_DestroyLogChannel(LogChannel* pChannel)
    {
        if (!pChannel)
        {
            return;
        }

        g_logger->RemoveDynamicLogChannel(pChannel);

        // owns allocation
        delete pChannel;
    }

} // extern "C"
