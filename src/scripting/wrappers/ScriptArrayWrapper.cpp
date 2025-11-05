#include <HyperionPch.hpp>
#ifdef HYP_SCRIPT

#include <script/vm/Value.hpp>
#include <script/vm/Array.hpp>

#include <core/reflection/ClassUtils.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

using Script_Array = Array<HypData, DynamicAllocator>;

HYP_API const Class* g_clsScript_Array = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(Script_Array, -1, 0, {})
    Method(NAME("Size"), &Type::Size),
    Method(NAME("PushBack"), +[](Script_Array& array, const HypData& arg) -> AnyRef
        {
            return AnyRef(array.PushBack(arg));
        }),
    Method(NAME("PopBack"), +[](Script_Array& array) -> HypData
        {
            Assert(!array.Empty());
            return array.PopBack();
        }),
    Method(NAME("Clear"), &Script_Array::Clear),
    Method(NAME("Resize"), &Script_Array::Resize),
    Method(NAME("Reserve"), &Script_Array::Reserve),
    Method(NAME("Empty"), &Script_Array::Empty),
    Method(NAME("Front"), +[](Script_Array& array) -> AnyRef
        {
            Assert(!array.Empty());
            return AnyRef(array.Front());
        }),
    Method(NAME("Back"), +[](Script_Array& array) -> AnyRef
        {
            Assert(!array.Empty());
            return AnyRef(array.Back());
        })
HYP_END_STRUCT
// clang-format on

} // namespace hyperion

#endif
