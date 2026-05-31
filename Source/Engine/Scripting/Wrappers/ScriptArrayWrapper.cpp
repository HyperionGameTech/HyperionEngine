#include <HyperionPch.hpp>

#ifdef HYP_SCRIPT

#include <Lang/VM/Value.hpp>
#include <Lang/VM/ScriptMemory.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Debug/Debug.hpp>

namespace Hyperion {

ENGINE_API const Class* g_clsGenericArrayWrapper = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(GenericArrayWrapper, -1, 0, {})
    Method(NAME("Size"), &Type::Size),
    Method(NAME("PushBack"), +[](GenericArrayWrapper& array, const BoxedValue& arg) -> AnyRef
        {
            return array.PushBack(BoxedValue(arg));
        })
HYP_END_STRUCT
// clang-format on

HYP_REGISTER_STATIC_CLASS(GenericArrayWrapper);

} // namespace Hyperion

#endif
