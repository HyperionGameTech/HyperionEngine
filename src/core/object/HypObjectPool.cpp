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

HypObjectContainerBase& HypObjectPool::ContainerMap::GetOrCreate(const HypClass* hypClass, HypObjectContainerBase* (*createFn)(const HypClass* hypClass))
{
    HYP_CORE_ASSERT(hypClass != nullptr);

    Mutex::Guard guard(m_mutex);

    auto it = m_map.FindIf([hypClass](const auto& element)
        {
            return element.first == hypClass;
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

    const TypeId typeId = hypClass->GetTypeId();
    HYP_CORE_ASSERT(typeId != TypeId::Void());

    container->m_typeId = typeId;
    container->m_hypClass = hypClass;

    return *m_map.EmplaceBack(hypClass, container).second;
}

HypObjectContainerBase& HypObjectPool::ContainerMap::Get(const HypClass* hypClass)
{
    HYP_CORE_ASSERT(hypClass != nullptr);

    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([hypClass](const auto& element)
        {
            return element.first == hypClass;
        });

    if (it == m_map.End())
    {
        HYP_FAIL("No object container for HypClass: %s", *hypClass->GetName());
    }

    HYP_CORE_ASSERT(it->second != nullptr);

    return *it->second;
}

HypObjectContainerBase* HypObjectPool::ContainerMap::TryGet(const HypClass* hypClass)
{
    HYP_CORE_ASSERT(hypClass != nullptr);

    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([hypClass](const auto& element)
        {
            return element.first == hypClass;
        });

    if (it == m_map.End())
    {
        return nullptr;
    }

    return it->second;
}

#pragma region HypObjectContainerBase

HypObjectContainerBase::HypObjectContainerBase(TypeId typeId, const HypClass* hypClass)
    : m_typeId(typeId),
      m_hypClass(hypClass),
      m_objectSize(hypClass != nullptr ? hypClass->GetSize() : 0),
      m_objectAlignment(hypClass != nullptr ? hypClass->GetAlignment() : 0)
{
    HYP_CORE_ASSERT(typeId != TypeId::Void());
}

#pragma endregion HypObjectContainerBase

} // namespace hyperion
