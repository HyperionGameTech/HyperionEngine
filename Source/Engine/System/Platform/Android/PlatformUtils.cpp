#include <SystemPch.hpp>

#include <unistd.h>
#include <limits.h>

#include <Core/containers/String.hpp>

namespace Hyperion {
namespace PlatformUtils {

ENGINE_API PlatformString GetExecutableAbsolutePath()
{
    char buffer[PATH_MAX];
    ssize_t result = readlink("/proc/self/exe", buffer, PATH_MAX - 1);

    if (result == -1)
    {
        return PlatformString();
    }

    buffer[result] = '\0';
    return PlatformString(buffer, buffer + result);
}

ENGINE_API bool IsOnBatteryPower()
{
    // TODO: implement for Android using BatteryManager
    return false;
}

ENGINE_API bool HasBattery()
{
    return true;
}

ENGINE_API uint64 GetSystemMemoryBytes()
{
    return uint64(sysconf(_SC_PHYS_PAGES)) * uint64(sysconf(_SC_PAGE_SIZE));
}

ENGINE_API uint32 GetLogicalCoreCount()
{
    const long numCores = sysconf(_SC_NPROCESSORS_ONLN);

    return numCores > 0 ? uint32(numCores) : 1;
}

ENGINE_API void InitializeNetwork()
{
}

} // namespace PlatformUtils
} // namespace Hyperion
