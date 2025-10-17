#ifdef HYP_SCRIPT

#include <script/vm/Value.hpp>
#include <script/vm/String.hpp>

#include <core/reflection/HypClassUtils.hpp>
#include <core/reflection/HypClassRegistry.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

HYP_API const HypClass* g_clsScript_String = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(Script_String, -1, 0, {})
    HypMethod(NAME("operator+"), +[](const Script_String& a, const Script_String& b) -> Script_String
        {
            return a + b;
        }),
    HypMethod(NAME("Length"), +[](const Script_String& str) -> uint64
        {
            return str.Length();
        })
HYP_END_STRUCT
// clang-format on

} // namespace hyperion

#endif