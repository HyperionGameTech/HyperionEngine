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

} // namespace PlatformUtils
} // namespace Hyperion
