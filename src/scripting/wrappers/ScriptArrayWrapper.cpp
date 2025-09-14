#ifdef HYP_SCRIPT

#include <script/vm/Value.hpp>

#include <core/containers/Array.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClassRegistry.hpp>

namespace hyperion {

using Script_Array = Array<Script_Value>;

// clang-format off
HYP_BEGIN_STRUCT(Script_Array, -1, 0, {})
    HypMethod(NAME("Size"), &Type::Size),
    HypMethod(NAME("PushBack"), static_cast<Script_Value&(Type::*)(const Script_Value&)>(&Type::PushBack)),
    HypMethod(NAME("PopBack"), &Type::PopBack),
    HypMethod(NAME("Clear"), &Type::Clear),
    HypMethod(NAME("Resize"), &Type::Resize),
    HypMethod(NAME("Reserve"), &Type::Reserve),
    HypMethod(NAME("Empty"), &Type::Empty),
    HypMethod(NAME("Front"), static_cast<Script_Value&(Type::*)()>(&Type::Front)),
    HypMethod(NAME("Back"), static_cast<Script_Value&(Type::*)()>(&Type::Back))
HYP_END_STRUCT

} // namespace hyperion

#endif