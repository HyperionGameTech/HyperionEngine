/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/ObjectPool.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Utilities/Format.hpp>

namespace Hyperion {

BoxedValue::~BoxedValue()
{
#ifdef HYP_SCRIPT
    AssertDebug(extData.gcIndex == INVALID_GC_INDEX,
        "BoxedValue being destroyed while still registered with the GC (index = {})",
        uint32(extData.gcIndex));

    extData.gcIndex = GARBAGE_GC_INDEX;
#endif
}

} // namespace Hyperion
