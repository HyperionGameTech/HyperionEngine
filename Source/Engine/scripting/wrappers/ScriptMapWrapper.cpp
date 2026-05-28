#include <HyperionPch.hpp>

#ifdef HYP_SCRIPT

#include <Lang/vm/Value.hpp>
#include <Lang/vm/Map.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

namespace Hyperion {

HYP_API const Class* g_clsScriptMap = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(ScriptMap, -1, 0, {})
    Method(NAME("FromArray"), +[](const BoxedValue& arrayBoxed) -> ScriptMap
        {
            ScriptMap map;

            if (Optional<GenericArrayWrapper&> arrayWrapperOpt = arrayBoxed.TryGet<GenericArrayWrapper>(); arrayWrapperOpt.HasValue())
            {
                GenericArrayWrapper& arrayWrapper = *arrayWrapperOpt;

                const size_t size = arrayWrapper.Size();
                map.Reserve(size);

                for (size_t i = 0; i < size; i++)
                {
                    GenericArrayWrapper& pair = arrayWrapper.GetElementAt(i).Get<GenericArrayWrapper>();

                    ScriptMapKey key;
                    pair.GetElementAt(0, key.key);
                    key.hash = key.key.value.GetHashCode().Value();

                    BoxedValue value;
                    pair.GetElementAt(1, value);

                    map.SetElement(std::move(key), std::move(value));
                }
            }

            return map;
        })
HYP_END_STRUCT
// clang-format on

HYP_REGISTER_STATIC_CLASS(ScriptMap);

} // namespace Hyperion

#endif
