/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/ObjectPool.hpp>

#include <Core/threading/Mutex.hpp>

#include <Core/utilities/Format.hpp>

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