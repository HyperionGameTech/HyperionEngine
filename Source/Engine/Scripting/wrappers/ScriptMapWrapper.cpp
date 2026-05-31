#include <HyperionPch.hpp>

#ifdef HYP_SCRIPT

#include <Lang/vm/Value.hpp>
#include <Lang/vm/Map.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

namespace Hyperion {

static const BoxedValue s_emptyBoxedValue = BoxedValue();

ENGINE_API const Class* g_clsScriptMap = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(ScriptMap, -1, 0, {})
    Method(NAME("operator[]"), +[](ScriptMap& map, const BoxedValue& key) -> const BoxedValue&
        {
            auto it = map.Find(ScriptMapKey { key });

            if (it == map.End())
            {
                return s_emptyBoxedValue;
            }

            return it->second;
        }),

    Method(NAME("operator[]="), +[](ScriptMap& map, const BoxedValue& key, const BoxedValue& value) -> const BoxedValue&
        {
            auto it = map.Find(ScriptMapKey { key });

            if (it == map.End())
            {
                return (map[ScriptMapKey { key }] = value);
            }
            else
            {
                it->second = value;

                return it->second;
            }
        }),

    Method(NAME("FromArray"), +[](const BoxedValue& arrayBoxed) -> ScriptMap
        {
            ScriptMap map;

            if (Optional<GenericArrayWrapper&> arrayWrapperOpt = arrayBoxed.TryGet<GenericArrayWrapper>(); arrayWrapperOpt.HasValue())
            {
                GenericArrayWrapper& arrayWrapper = *arrayWrapperOpt;

                const size_t arraySize = arrayWrapper.Size();
                map.Reserve(arraySize);

                for (size_t i = 0; i < arraySize; i++)
                {
                    BoxedValue& elem = *Deref(arrayWrapper.GetElementAt(i).GetUnchecked<BoxedValue>());

                    Optional<GenericArrayWrapper&> pairOpt = elem.TryGet<GenericArrayWrapper>();

                    AssertDebug(pairOpt.HasValue(), "Expected GenericArrayWrapper for element type. Got: {}", elem.GetTypeInfo()->name);

                    if (!pairOpt.HasValue())
                    {
                        continue;
                    }

                    GenericArrayWrapper& pair = pairOpt.Get();

                    ScriptMapKey key;
                    pair.GetElementAt(0, key.key);

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
