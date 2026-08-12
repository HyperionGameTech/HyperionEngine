/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Scripting/Strata/StrataMarshal.hpp>

#include <Core/Memory/Pool/Pool.hpp>

namespace Hyperion {
namespace Strata {

static Pool& GetPool()
{
    static Pool pool { 1 * 1024 * 1024, PF_THREAD_SAFE | PF_FALLBACK };

    return pool;
}

void* Alloc(size_t size)
{
    return GetPool().Allocate(size);
}

void Free(void* ptr)
{
    GetPool().Free(ptr);
}

char* AllocReturnString(const char* data, size_t size)
{
    char* buffer = reinterpret_cast<char*>(GetPool().Allocate(size + 1, 1));

    if (size > 0 && data != nullptr)
    {
        Memory::Copy(buffer, data, size);
    }

    buffer[size] = '\0';

    return buffer;
}

} // namespace Strata
} // namespace Hyperion
