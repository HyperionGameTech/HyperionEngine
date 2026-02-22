/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Core/memory/MemoryPool.hpp>

#include <Core/threading/Mutex.hpp>

#include <Core/containers/Array.hpp>

namespace Hyperion {
namespace memory {

#pragma region MemoryPoolManager

class MemoryPoolManager
{
public:
    void RegisterPool(MemoryPoolBase* pool, SizeType (*getNumAllocatedBytes)(MemoryPoolBase*))
    {
        HYP_CORE_ASSERT(pool != nullptr);
        HYP_CORE_ASSERT(getNumAllocatedBytes != nullptr);

        Mutex::Guard guard(m_mutex);

        for (auto it = m_registeredPools.Begin(); it != m_registeredPools.End(); ++it)
        {
            if (it->first == nullptr)
            {
                *it = { pool, getNumAllocatedBytes };

                return;
            }
        }

        m_registeredPools.EmplaceBack(pool, getNumAllocatedBytes);
    }

    void UnregisterPool(MemoryPoolBase* pool)
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_registeredPools.FindIf([pool](const auto& item)
            {
                return item.first == pool;
            });

        if (it != m_registeredPools.End())
        {
            *it = {};
        }
    }

    void RemoveEmpty()
    {
        for (auto it = m_registeredPools.Begin(); it != m_registeredPools.End();)
        {
            if (it->first == nullptr)
            {
                it = m_registeredPools.Erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void CalculateMemoryUsage(Array<Pair<MemoryPoolBase*, SizeType>>& outBytesPerPool)
    {
        Mutex::Guard guard(m_mutex);

        outBytesPerPool.Reserve(m_registeredPools.Size());

        for (SizeType i = 0; i < m_registeredPools.Size(); i++)
        {
            if (!m_registeredPools[i].first)
            {
                continue;
            }

            outBytesPerPool.EmplaceBack(m_registeredPools[i].first, m_registeredPools[i].second(m_registeredPools[i].first));
        }
    }

private:
    Mutex m_mutex;
    Array<Pair<MemoryPoolBase*, SizeType (*)(MemoryPoolBase*)>> m_registeredPools;
};

HYP_API MemoryPoolManager& GetMemoryPoolManager()
{
    static MemoryPoolManager memoryPoolManager;

    return memoryPoolManager;
}

HYP_API void CalculateMemoryUsagePerPool(Array<Pair<MemoryPoolBase*, SizeType>>& outBytesPerPool)
{
    GetMemoryPoolManager().CalculateMemoryUsage(outBytesPerPool);
}

#pragma endregion MemoryPoolManager

#pragma region MemoryPoolBase

MemoryPoolBase::MemoryPoolBase(Name poolName, ThreadId ownerThreadId, SizeType (*getNumAllocatedBytes)(MemoryPoolBase*))
    : m_poolName(poolName),
      m_ownerThreadId(ownerThreadId)
{
    GetMemoryPoolManager().RegisterPool(this, getNumAllocatedBytes);
}

MemoryPoolBase::~MemoryPoolBase()
{
    GetMemoryPoolManager().UnregisterPool(this);
}

#pragma endregion MemoryPoolBase

} // namespace memory
} // namespace Hyperion