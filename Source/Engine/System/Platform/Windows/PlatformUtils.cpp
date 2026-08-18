#include <SystemPch.hpp>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winbase.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <Core/Containers/String.hpp>

#include <mutex>

namespace Hyperion {
namespace PlatformUtils {

ENGINE_API PlatformString GetExecutableAbsolutePath()
{
    wchar_t buffer[MAX_PATH];
    DWORD result = GetModuleFileNameW(nullptr, buffer, MAX_PATH);

    if (result == 0 || result >= MAX_PATH)
    {
        // If failed or buffer too small, try with dynamic allocation
        DWORD requiredSize = GetModuleFileNameW(nullptr, nullptr, 0);
        if (requiredSize == 0)
        {
            return PlatformString();
        }

        Array<wchar_t> dynBuffer;
        dynBuffer.Resize(requiredSize);
        result = GetModuleFileNameW(nullptr, dynBuffer.Data(), requiredSize);

        if (result == 0)
        {
            return PlatformString();
        }

        return PlatformString(dynBuffer.Data(), dynBuffer.Data() + result);
    }

    return PlatformString(buffer, buffer + result);
}

ENGINE_API bool IsOnBatteryPower()
{
    SYSTEM_POWER_STATUS powerStatus;
    if (!GetSystemPowerStatus(&powerStatus))
    {
        return false;
    }

    return powerStatus.ACLineStatus == 0;
}

ENGINE_API void InitializeNetwork()
{
    static std::once_flag s_onceFlag;

    std::call_once(s_onceFlag, []
        {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        });
}

} // namespace PlatformUtils
} // namespace Hyperion
