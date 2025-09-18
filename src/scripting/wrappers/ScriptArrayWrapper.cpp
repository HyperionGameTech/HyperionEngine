#ifdef HYP_SCRIPT

#include <script/vm/Value.hpp>
#include <script/vm/Array.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

// Script_Value has same alignof HypData and HypData is stored at offset 0,
// so we reinterpret_cast between the two types as HypData is what is passed to HypMethod
// calls.
static inline const Script_Value& ToScriptValue(const HypData& data)
{
    return *reinterpret_cast<const Script_Value*>(&data);
}

static inline Script_Value& ToScriptValue(HypData& data)
{
    return *reinterpret_cast<Script_Value*>(&data);
}

// clang-format off
HYP_BEGIN_STRUCT(Script_Array, -1, 0, {})
    HypMethod(NAME("Size"), &Type::Size),
    HypMethod(NAME("PushBack"), +[](Script_Array& array, const HypData& arg) -> HypData&
        {
            DebugLog(LogType::Debug, "Push value to array: %s\n", ToScriptValue(arg).ToString().Data());
            return *array.PushBack(ToScriptValue(arg)).GetHypData();
        }),
    HypMethod(NAME("PopBack"), +[](Script_Array& array) -> HypData
        {
            Assert(!array.Empty());
            return std::move(*array.PopBack().GetHypData());
        }),
    HypMethod(NAME("Clear"), &Script_Array::Clear),
    HypMethod(NAME("Resize"), &Script_Array::Resize),
    HypMethod(NAME("Reserve"), &Script_Array::Reserve),
    HypMethod(NAME("Empty"), &Script_Array::Empty),
    HypMethod(NAME("Front"), +[](Script_Array& array) -> HypData&
        {
            Assert(!array.Empty());
            return *array.Front().GetHypData();
        }),
    HypMethod(NAME("Back"), +[](Script_Array& array) -> HypData&
        {
            Assert(!array.Empty());
            return *array.Back().GetHypData();
        })
HYP_END_STRUCT
// clang-format on

} // namespace hyperion

#endif