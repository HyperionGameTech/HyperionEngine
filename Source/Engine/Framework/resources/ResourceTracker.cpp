/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Framework/resources/ResourceTracker.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/memory/pool/Pool.hpp>

namespace Hyperion {

ENGINE_API const TypeInfo& Class_GetTypeInfo(const Class& cls)
{
    return *cls.GetTypeInfo();
}

} // namespace Hyperion
