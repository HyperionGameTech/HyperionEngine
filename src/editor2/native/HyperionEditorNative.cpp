#include <HyperionPch.hpp>

#include <core/logging/Logger.hpp>
#include <console/ConsoleCommandManager.hpp>

namespace hyperion {

using LogCallback = void (*)(const char* channel, int level, double timestamp, const char* message);

LogCallback g_logCallback = nullptr;
int g_logRedirectId = -1;

static void HandleLogMessage(void* context, const LogChannel& channel, const LogMessage& message)
{
    if (g_logCallback)
    {
        String text;
        for (const auto& chunk : message.chunks)
        {
            text.Append(chunk);
        }

        g_logCallback(channel.name.LookupString(), (int)message.level, (double)message.timestamp, text.Data());
    }
}

extern "C"
{

    HYP_EXPORT void Editor_RegisterLogCallback(LogCallback callback)
    {
        g_logCallback = callback;

        if (g_logRedirectId == -1)
        {
            g_logRedirectId = g_logger->GetOutputStream()->AddRedirect(
                Bitset(~0u), // All channels
                nullptr,
                HandleLogMessage,
                HandleLogMessage // Use same handler for errors for now
            );
        }
    }

    HYP_EXPORT void Editor_ExecuteConsoleCommand(const char* command)
    {
        if (!command)
            return;

        ConsoleCommandManager::GetInstance().ExecuteCommand(command);
    }

} // extern "C"

} // namespace hyperion
