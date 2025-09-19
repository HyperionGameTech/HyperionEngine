#ifdef HYP_SCRIPT

#include <script/vm/Value.hpp>
#include <script/vm/Array.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

// clang-format off
HYP_BEGIN_STRUCT(Script_Array, -1, 0, {})
    HypMethod(NAME("Size"), &Type::Size),
    HypMethod(NAME("PushBack"), static_cast<HypData& (Script_Array::*)(const HypData&)>(&Script_Array::PushBack)),
    HypMethod(NAME("PopBack"), static_cast<HypData (Script_Array::*)()>(&Script_Array::PopBack)),
    HypMethod(NAME("Clear"), &Script_Array::Clear),
    HypMethod(NAME("Resize"), &Script_Array::Resize),
    HypMethod(NAME("Reserve"), &Script_Array::Reserve),
    HypMethod(NAME("Empty"), &Script_Array::Empty),
    HypMethod(NAME("Front"), static_cast<HypData& (Script_Array::*)()>(&Script_Array::Front)),
    HypMethod(NAME("Back"), static_cast<HypData& (Script_Array::*)()>(&Script_Array::Back))
HYP_END_STRUCT
// clang-format on

} // namespace hyperion

#endif