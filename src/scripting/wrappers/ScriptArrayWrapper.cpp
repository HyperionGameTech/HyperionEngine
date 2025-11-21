#include <HyperionPch.hpp>
#ifdef HYP_SCRIPT

#include <script/vm/Value.hpp>
#include <script/vm/Array.hpp>

#include <core/reflection/ClassUtils.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

using ScriptArray = Array<HypData, DynamicAllocator>;

HYP_API const Class* g_clsScriptArray = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(ScriptArray, -1, 0, {})
    Method(NAME("Size"), &Type::Size),
    Method(NAME("PushBack"), +[](ScriptArray& array, const HypData& arg) -> AnyRef
        {
            return AnyRef(array.PushBack(arg));
        }),
    Method(NAME("PopBack"), +[](ScriptArray& array) -> HypData
        {
            Assert(!array.Empty());
            return array.PopBack();
        }),
    Method(NAME("Clear"), &ScriptArray::Clear),
    Method(NAME("Resize"), &ScriptArray::Resize),
    Method(NAME("Reserve"), &ScriptArray::Reserve),
    Method(NAME("Empty"), &ScriptArray::Empty),
    Method(NAME("Front"), +[](ScriptArray& array) -> AnyRef
        {
            Assert(!array.Empty());
            return AnyRef(array.Front());
        }),
    Method(NAME("Back"), +[](ScriptArray& array) -> AnyRef
        {
            Assert(!array.Empty());
            return AnyRef(array.Back());
        })
HYP_END_STRUCT
// clang-format on

HYP_REGISTER_STATIC_CLASS(ScriptArray);

} // namespace hyperion

#endif
