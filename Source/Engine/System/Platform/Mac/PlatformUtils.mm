#include <SystemPch.hpp>

#include <mach-o/dyld.h>

#include <Core/Containers/String.hpp>

namespace Hyperion {
namespace PlatformUtils {

ENGINE_API PlatformString GetExecutableAbsolutePath()
{
    char buffer[PATH_MAX];
    uint32_t bufferSize = PATH_MAX;

    if (_NSGetExecutablePath(buffer, &bufferSize) != 0)
    {
        // Buffer too small, allocate dynamically
        Array<char> dynBuffer;
        dynBuffer.Resize(bufferSize);

        if (_NSGetExecutablePath(dynBuffer.Data(), &bufferSize) != 0)
        {
            return PlatformString();
        }

        return PlatformString(dynBuffer.Data(), dynBuffer.Data() + bufferSize - 1);
    }

    // Get the actual length of the path
    size_t pathLength = std::strlen(buffer);
    return PlatformString(buffer, buffer + pathLength);
}

} // namespace PlatformUtils
} // namespace Hyperion
