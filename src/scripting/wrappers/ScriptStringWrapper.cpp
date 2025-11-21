#include <HyperionPch.hpp>
#ifdef HYP_SCRIPT

#include <script/vm/Value.hpp>
#include <script/vm/String.hpp>
#include <script/vm/Array.hpp>

#include <core/reflection/ClassUtils.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

HYP_API const Class* g_clsScript_String = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(Script_String, -1, 0, {})
    Method(NAME("operator+"), +[](const Script_String& a, const Script_String& b) -> Script_String
        {
            return a + b;
        }),
    Method(NAME("Length"), +[](const Script_String& str) -> uint64
        {
            return str.Length();
        }),
    Method(NAME("Join"), +[](const Array<HypData>& elems, const Script_String& sep) -> Script_String
        {
            Script_String result;

            for (SizeType i = 0; i < elems.Size(); ++i)
            {
                result += ToString(elems[i]);

                if (i != elems.Size() - 1)
                {
                    result += sep;
                }
            }

            return result;
        }),
    Method(NAME("$invoke"), +[](const HypData& value) -> Script_String
        {
            return ToString(value);
        })
HYP_END_STRUCT
// clang-format on

HYP_REGISTER_STATIC_CLASS(Script_String);

} // namespace hyperion

#endif
