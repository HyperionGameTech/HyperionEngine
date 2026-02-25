/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/ObjectPool.hpp>

#include <Core/threading/Mutex.hpp>

#include <Core/utilities/Format.hpp>

namespace Hyperion {

static HashMap<TypeId, BoxedValueSerializeFunction>& GetBoxedValueSerializeFunctionMap()
{
    static HashMap<TypeId, BoxedValueSerializeFunction> s_serializeFunctions;

    return s_serializeFunctions;
}

static Mutex& GetBoxedValueSerializeFunctionMapMutex()
{
    static Mutex s_serializeFunctionsMutex;

    return s_serializeFunctionsMutex;
}

HYP_API BoxedValueSerializeFunction GetBoxedValueSerializeFunction(TypeId typeId)
{
    Mutex::Guard guard(GetBoxedValueSerializeFunctionMapMutex());

    auto& map = GetBoxedValueSerializeFunctionMap();

    const auto it = map.Find(typeId);

    if (it != map.End())
    {
        return it->second;
    }

    return nullptr;
}

HYP_API void RegisterBoxedValueSerializeFunction(TypeId typeId, BoxedValueSerializeFunction func)
{
    Assert(func != nullptr);

    Mutex::Guard guard(GetBoxedValueSerializeFunctionMapMutex());

    auto& map = GetBoxedValueSerializeFunctionMap();
    map[typeId] = func;
}

} // namespace Hyperion