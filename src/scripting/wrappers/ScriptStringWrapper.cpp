#include <HyperionPch.hpp>
#ifdef HYP_SCRIPT

#include <script/vm/Value.hpp>
#include <script/vm/String.hpp>
#include <script/vm/Array.hpp>

#include <core/reflection/ClassUtils.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <core/debug/Debug.hpp>

namespace Hyperion {

HYP_API const Class* g_clsString = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(String, -1, 0, {})
    Method(NAME("operator+"), +[](const String& a, const String& b) -> String
        {
            return a + b;
        }),
    Method(NAME("Length"), +[](const String& str) -> uint64
        {
            return str.Length();
        }),
    Method(NAME("Join"), +[](const Array<BoxedValue>& elems, const String& sep) -> String
        {
            String result;

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
    Method(NAME("$invoke"), +[](const BoxedValue& value) -> String
        {
            return ToString(value);
        })
HYP_END_STRUCT
// clang-format on

HYP_REGISTER_STATIC_CLASS(String);

} // namespace Hyperion

#endif
