/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/reflection/HypObjectPool.hpp>
#include <core/reflection/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
#include <engine/EngineMemory.hpp>
#endif

namespace hyperion {

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
HYP_API extern Pool* g_objectPool;
#endif

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

static Spinlock<MPMC>& GetLock()
{
    static volatile int64 s_lockValue = 0;
    static Spinlock<MPMC> s_lock { &s_lockValue };

    return s_lock;
}

static Pool* GetPool()
{
#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    return g_objectPool;
#else
    static Pool s_globalPool { 16 * 1024 * 1024 };
    return &s_globalPool;
#endif
}

static Pool* GetPoolForClass(const HypClass* hypClass)
{
    HYP_CORE_ASSERT(hypClass != nullptr);

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    Pool* const* pool = g_enginePools[hypClass->GetEnginePoolName()];
    HYP_CORE_ASSERT(pool != nullptr && *pool != nullptr, "Engine pool not found for %u", uint32(hypClass->GetEnginePoolName()));

    return *pool;
#endif

    return GetPool();
}

#pragma region HypObjectContainerBase

HypObjectContainerBase::HypObjectContainerBase(TypeId typeId, const HypClass* hypClass)
    : m_typeId(typeId),
      m_hypClass(hypClass)
{
    HYP_CORE_ASSERT(typeId != TypeId::Void());
}

Pool* HypObjectContainerBase::GetPool() const
{
    return GetPoolForClass(m_hypClass);
}

void HypObjectContainerBase::LockPoolOrThreadAssert(Pool* pool, LockGuard& outGuard, int flags)
{
    HYP_CORE_ASSERT(pool != nullptr);

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    if (pool->GetFlags() & PF_THREAD_SAFE)
    {
#endif
        Spinlock<MPMC>& lock = GetLock();
        lock.Lock();

        outGuard.lock = &lock;
        outGuard.flags = flags;

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
        return;
    }

    /*if (!Threads::IsOnThread(EngineMemory_GetPoolThreadId(poolName)))
    {
        HYP_LOG(Core, Warning, "Create/destroying object of type {} from thread: {} but its pool is owned by thread: {}",
            m_hypClass->GetName(),
            Threads::CurrentThreadId().GetName(),
            EngineMemory_GetPoolThreadId(poolName).GetName());
    }*/

    // Threads::AssertOnThread(EngineMemory_GetPoolThreadId(poolName), "HypObject can only be created/destroyed from its owning pool thread");
#endif
}

#pragma endregion HypObjectContainerBase

} // namespace hyperion
