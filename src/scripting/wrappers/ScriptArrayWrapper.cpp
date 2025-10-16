#ifdef HYP_SCRIPT

#include <script/vm/Value.hpp>
#include <script/vm/Array.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

using Script_Array = Array<HypData>;

HYP_API const HypClass* g_clsScript_Array = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(Script_Array, -1, 0, {})
    HypMethod(NAME("Size"), &Type::Size),
    HypMethod(NAME("PushBack"), +[](Script_Array& array, const HypData& arg) -> AnyRef
        {
            return AnyRef(array.PushBack(arg));
        }),
    HypMethod(NAME("PopBack"), +[](Script_Array& array) -> HypData
        {
            Assert(!array.Empty());
            return array.PopBack();
        }),
    HypMethod(NAME("Clear"), &Script_Array::Clear),
    HypMethod(NAME("Resize"), &Script_Array::Resize),
    HypMethod(NAME("Reserve"), &Script_Array::Reserve),
    HypMethod(NAME("Empty"), &Script_Array::Empty),
    HypMethod(NAME("Front"), +[](Script_Array& array) -> AnyRef
        {
            Assert(!array.Empty());
            return AnyRef(array.Front());
        }),
    HypMethod(NAME("Back"), +[](Script_Array& array) -> AnyRef
        {
            Assert(!array.Empty());
            return AnyRef(array.Back());
        })
HYP_END_STRUCT
// clang-format on

} // namespace hyperion

#endif