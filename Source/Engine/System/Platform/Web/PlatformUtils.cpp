/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <Core/Containers/String.hpp>

#include <emscripten/threading.h>
#include <emscripten/heap.h>

namespace Hyperion {
namespace PlatformUtils {

ENGINE_API PlatformString GetExecutableAbsolutePath()
{
    return PlatformString("/hyperion/hyperion-sample");
}

ENGINE_API bool IsOnBatteryPower()
{
    return false;
}

ENGINE_API bool HasBattery()
{
    return false;
}

ENGINE_API uint64 GetSystemMemoryBytes()
{
    return uint64(emscripten_get_heap_max());
}

ENGINE_API uint32 GetLogicalCoreCount()
{
    const int numCores = emscripten_num_logical_cores();

    return numCores > 0 ? uint32(numCores) : 1;
}

ENGINE_API void InitializeNetwork()
{
}

} // namespace PlatformUtils
} // namespace Hyperion
