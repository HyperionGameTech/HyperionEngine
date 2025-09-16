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
    static HypObjectPool::ContainerMap s_objectContainerMap;

    return s_objectContainerMap;
}

HypObjectContainerBase& HypObjectPool::ContainerMap::GetOrCreate(TypeId typeId, const HypClass* hypClass, HypObjectContainerBase* (*createFn)(const HypClass* hypClass))
{
    HYP_CORE_ASSERT(hypClass != nullptr);
    HYP_CORE_ASSERT(hypClass->GetTypeId() != TypeId::Void());
    HYP_CORE_ASSERT(hypClass->GetSize() != 0 && hypClass->GetAlignment() != 0);

    Mutex::Guard guard(m_mutex);

    auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it != m_map.End())
    {
        if (it->second == nullptr)
        {
            it->second = createFn(hypClass);

            HYP_CORE_ASSERT(it->second != nullptr);
        }

        return *it->second;
    }

    HypObjectContainerBase* container = createFn(hypClass);
    HYP_CORE_ASSERT(container != nullptr);

    container->m_typeId = typeId;
    container->m_hypClass = hypClass;

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
        HYP_FAIL("No object container for HypClass: %s", LookupTypeName(typeId));
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
        HYP_BREAKPOINT;

        return nullptr;
    }

    return it->second;
}

#pragma region HypObjectContainerBase

HypObjectContainerBase::HypObjectContainerBase(TypeId typeId, const HypClass* hypClass)
    : m_typeId(typeId),
      m_hypClass(hypClass)
{
    HYP_CORE_ASSERT(typeId != TypeId::Void());
}

#pragma endregion HypObjectContainerBase

} // namespace hyperion
