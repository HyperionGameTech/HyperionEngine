#include <HyperionPch.hpp>

#ifdef HYP_SCRIPT

#include <script/vm/Value.hpp>
#include <script/vm/String.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/debug/Debug.hpp>

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
    Method(NAME("Join"), +[](const GenericArrayWrapper& elems, const String& sep) -> String
        {
            String result;

            const SizeType size = elems.Size();

            for (SizeType i = 0; i < size; ++i)
            {
                AnyRef elem = const_cast<GenericArrayWrapper&>(elems).GetElementAt(i);

                if (elem.Is<BoxedValue>())
                {
                    BoxedValue& boxed = elem.Get<BoxedValue>();

                    result += ToString(boxed);
                }
                else
                {
                    BoxedValue boxed = BoxedValue(elem);

                    result += ToString(boxed);
                }

                if (i != size - 1)
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
