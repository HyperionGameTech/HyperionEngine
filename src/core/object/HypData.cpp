/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>
#include <core/object/HypObjectPool.hpp>

#include <core/threading/Mutex.hpp>

#include <core/utilities/Format.hpp>

namespace hyperion {

static HashMap<TypeId, HypDataSerializeFunction>& GetHypDataSerializeFunctionMap()
{
    static HashMap<TypeId, HypDataSerializeFunction> s_serializeFunctions;

    return s_serializeFunctions;
}

static Mutex& GetHypDataSerializeFunctionMapMutex()
{
    static Mutex s_serializeFunctionsMutex;

    return s_serializeFunctionsMutex;
}

HYP_API HypDataSerializeFunction GetHypDataSerializeFunction(TypeId typeId)
{
    Mutex::Guard guard(GetHypDataSerializeFunctionMapMutex());

    auto& map = GetHypDataSerializeFunctionMap();

    const auto it = map.Find(typeId);

    if (it != map.End())
    {
        return it->second;
    }

    return nullptr;
}

HYP_API void RegisterHypDataSerializeFunction(TypeId typeId, HypDataSerializeFunction func)
{
    Assert(func != nullptr);

    Mutex::Guard guard(GetHypDataSerializeFunctionMapMutex());

    auto& map = GetHypDataSerializeFunctionMap();
    map[typeId] = func;
}

HYP_API void SetHypDataFromReference(HypData& hypData, AnyRef ref)
{
    HypDataHelper<AnyRef> {}.Set(hypData, ref);
}

} // namespace hyperion