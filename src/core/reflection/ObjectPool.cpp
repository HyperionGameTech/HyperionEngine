/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/reflection/ObjectPool.hpp>
#include <core/reflection/Class.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
#include <engine/EngineMemory.hpp>
#endif

namespace Hyperion {

HYP_API void ReleaseObject(ObjectHeader* header)
{
    AssertDebug(header != nullptr);

    const Class* cls = header->cls;
    AssertDebug(cls != nullptr);

    ObjectContainerBase* container = cls->GetObjectContainer();
    AssertDebug(container != nullptr, "Class has no ObjectContainer");

    cls->GetObjectContainer()->Release(header);
}

ObjectPool::ContainerMap::~ContainerMap()
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

ObjectPool::ContainerMap& ObjectPool::GetObjectContainerMap()
{
    static ObjectPool::ContainerMap s_objectContainerMap;

    return s_objectContainerMap;
}

ObjectContainerBase& ObjectPool::ContainerMap::GetOrCreate(TypeId typeId, const Class* cls, ObjectContainerBase* (*createFn)(const Class* cls))
{
    AssertDebug(cls != nullptr);
    AssertDebug(cls->GetTypeId() != TypeId::Void());
    AssertDebug(cls->GetSize() != 0 && cls->GetAlignment() != 0);

    Mutex::Guard guard(m_mutex);

    auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it != m_map.End())
    {
        if (it->second == nullptr)
        {
            it->second = createFn(cls);

            AssertDebug(it->second != nullptr);
        }

        return *it->second;
    }

    ObjectContainerBase* container = createFn(cls);
    AssertDebug(container != nullptr);

    container->m_typeId = typeId;
    container->m_class = cls;

    return *m_map.EmplaceBack(typeId, container).second;
}

ObjectContainerBase& ObjectPool::ContainerMap::Get(TypeId typeId)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it == m_map.End())
    {
        HYP_FAIL("No object container for Class: {}", LookupTypeName(typeId));
    }

    AssertDebug(it->second != nullptr);

    return *it->second;
}

ObjectContainerBase* ObjectPool::ContainerMap::TryGet(TypeId typeId)
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

#pragma region ObjectContainerBase

ObjectContainerBase::ObjectContainerBase(TypeId typeId, const Class* cls)
    : m_typeId(typeId),
      m_class(cls),
      m_pool(nullptr)
{
    AssertDebug(typeId != TypeId::Void());
}

void ObjectContainerBase::LockIfNeeded(TLockGuard<AtomicFlag>& outGuard, int flags)
{
    AssertDebug(m_pool != nullptr);

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    if (m_pool == g_objectPool || (m_pool->GetFlags() & PF_THREAD_SAFE))
    {
#endif
        outGuard.Reset(m_atomicFlag);

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
        return;
    }

    /*if (!IsOnThread(EngineMemory_GetPoolThreadId(poolName)))
    {
        HYP_LOG(Core, Warning, "Create/destroying object of type {} from thread: {} but its pool is owned by thread: {}",
            m_class->GetName(),
            CurrentThreadId().GetName(),
            EngineMemory_GetPoolThreadId(poolName).GetName());
    }*/

    // AssertOnThread(EngineMemory_GetPoolThreadId(poolName), "Object can only be created/destroyed from its owning pool thread");
#endif
}

#pragma endregion ObjectContainerBase

} // namespace Hyperion
