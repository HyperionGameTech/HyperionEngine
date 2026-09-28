/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>

#include <dirent.h>
#include <limits.h>
#include <unistd.h>

namespace Hyperion {
namespace PlatformUtils {

ENGINE_API PlatformString GetExecutableAbsolutePath()
{
    char buffer[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer));

    // readlink doesn't null terminate, and a full buffer means the path may have been truncated
    if (length <= 0 || length >= ssize_t(sizeof(buffer)))
    {
        return PlatformString();
    }

    return PlatformString(buffer, buffer + length);
}

// Reads the first line of a small sysfs attribute file, without the trailing newline
static bool ReadSysfsValue(const char* directory, const char* attribute, char* outValue, size_t outValueSize)
{
    char path[PATH_MAX];
    std::snprintf(path, sizeof(path), "/sys/class/power_supply/%s/%s", directory, attribute);

    FILE* file = std::fopen(path, "r");

    if (!file)
    {
        return false;
    }

    const bool success = std::fgets(outValue, int(outValueSize), file) != nullptr;
    std::fclose(file);

    if (success)
    {
        outValue[std::strcspn(outValue, "\r\n")] = '\0';
    }

    return success;
}

static bool QueryIsOnBatteryPower()
{
    DIR* powerSupplies = opendir("/sys/class/power_supply");

    if (!powerSupplies)
    {
        return false;
    }

    bool hasExternalPower = false;
    bool hasDischargingBattery = false;

    while (dirent* entry = readdir(powerSupplies))
    {
        if (entry->d_name[0] == '.')
        {
            continue;
        }

        char type[64];

        if (!ReadSysfsValue(entry->d_name, "type", type, sizeof(type)))
        {
            continue;
        }

        if (std::strcmp(type, "Battery") == 0)
        {
            // peripherals (mice, controllers) also report batteries, only count ones powering the system
            char scope[64];

            if (ReadSysfsValue(entry->d_name, "scope", scope, sizeof(scope)) && std::strcmp(scope, "Device") == 0)
            {
                continue;
            }

            char status[64];

            if (ReadSysfsValue(entry->d_name, "status", status, sizeof(status)) && std::strcmp(status, "Discharging") == 0)
            {
                hasDischargingBattery = true;
            }
        }
        else
        {
            // Mains, USB, USB_PD etc.
            char online[16];

            if (ReadSysfsValue(entry->d_name, "online", online, sizeof(online)) && std::strcmp(online, "1") == 0)
            {
                hasExternalPower = true;
            }
        }
    }

    closedir(powerSupplies);

    return hasDischargingBattery && !hasExternalPower;
}

ENGINE_API bool IsOnBatteryPower()
{
    // Called every frame by the render thread, so only re-read sysfs every few seconds
    static constexpr int64 RefreshIntervalMs = 5000;

    static AtomicVar<int64> s_nextRefreshTimeMs { 0 };
    static AtomicVar<bool> s_isOnBatteryPower { false };

    const int64 nowMs = int64(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
            .count());

    if (nowMs >= s_nextRefreshTimeMs.Get(MemoryOrder::ACQUIRE))
    {
        s_nextRefreshTimeMs.Set(nowMs + RefreshIntervalMs, MemoryOrder::RELEASE);
        s_isOnBatteryPower.Set(QueryIsOnBatteryPower(), MemoryOrder::RELAXED);
    }

    return s_isOnBatteryPower.Get(MemoryOrder::RELAXED);
}

ENGINE_API void InitializeNetwork()
{
}

} // namespace PlatformUtils
} // namespace Hyperion
