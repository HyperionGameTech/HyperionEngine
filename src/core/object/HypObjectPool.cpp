/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/object/HypObjectPool.hpp>
#include <core/object/HypClass.hpp>

namespace hyperion {

HYP_API void ReleaseHypObject(HypObjectHeader* header)
{
    HYP_CORE_ASSERT(header != nullptr);

    const HypClass* hypClass = header->hypClass;
    HYP_CORE_ASSERT(hypClass != nullptr);

    HypObjectContainerBase* container = hypClass->GetObjectContainer();
    HYP_CORE_ASSERT(container != nullptr, "HypClass has no HypObjectContainer");

    hypClass->GetObjectContainer()->Release(header);
}

static HypObjectPool::ContainerMap g_objectContainerMap {};

HypObjectPool::ContainerMap::~ContainerMap()
{
    for (auto& it : m_map)
    {
        if (it.second != nullptr)
        {
            delete it.second;
            it.second = nullptr;
        }
    }
}

HypObjectPool::ContainerMap& HypObjectPool::GetObjectContainerMap()
{
    return g_objectContainerMap;
}

HypObjectContainerBase& HypObjectPool::ContainerMap::GetOrCreate(TypeId typeId, HypObjectContainerBase* (*createFn)(void))
{
    Mutex::Guard guard(m_mutex);

    auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it != m_map.End())
    {
        if (it->second == nullptr)
        {
            it->second = createFn();

            HYP_CORE_ASSERT(it->second != nullptr);
        }

        return *it->second;
    }

    HypObjectContainerBase* container = createFn();
    HYP_CORE_ASSERT(container != nullptr);

    container->m_typeId = typeId;

    return *m_map.EmplaceBack(typeId, container).second;
}

HypObjectContainerBase& HypObjectPool::ContainerMap::Get(TypeId typeId)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it == m_map.End())
    {
        HYP_FAIL("No object container for TypeId: %u", typeId.Value());
    }

    HYP_CORE_ASSERT(it->second != nullptr);

    return *it->second;
}

HypObjectContainerBase* HypObjectPool::ContainerMap::TryGet(TypeId typeId)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it == m_map.End())
    {
        return nullptr;
    }

    return it->second;
}

} // namespace hyperion
