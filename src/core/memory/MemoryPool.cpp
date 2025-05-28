/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/memory/MemoryPool.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/Array.hpp>

namespace hyperion {
namespace memory {

#pragma region MemoryPoolManager

class MemoryPoolManager
{
public:
    void RegisterPool(MemoryPoolBase* pool, SizeType (*get_num_allocated_bytes)(MemoryPoolBase*))
    {
        AssertDebug(pool != nullptr);
        AssertDebug(get_num_allocated_bytes != nullptr);

        Mutex::Guard guard(m_mutex);

        for (auto it = m_registered_pools.Begin(); it != m_registered_pools.End(); ++it)
        {
            if (it->first == nullptr)
            {
                *it = { pool, get_num_allocated_bytes };

                return;
            }
        }

        m_registered_pools.EmplaceBack(pool, get_num_allocated_bytes);
    }

    void UnregisterPool(MemoryPoolBase* pool)
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_registered_pools.FindIf([pool](const auto& item)
            {
                return item.first == pool;
            });

        if (it != m_registered_pools.End())
        {
            *it = {};
        }
    }

    void RemoveEmpty()
    {
        for (auto it = m_registered_pools.Begin(); it != m_registered_pools.End();)
        {
            if (it->first == nullptr)
            {
                it = m_registered_pools.Erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void CalculateMemoryUsage(Array<SizeType>& out_bytes_per_pool)
    {
        Mutex::Guard guard(m_mutex);

        out_bytes_per_pool.Reserve(m_registered_pools.Size());

        for (SizeType i = 0; i < m_registered_pools.Size(); i++)
        {
            if (!m_registered_pools[i].first)
            {
                continue;
            }

            out_bytes_per_pool.PushBack(m_registered_pools[i].second(m_registered_pools[i].first));
        }
    }

private:
    Mutex m_mutex;
    Array<Pair<MemoryPoolBase*, SizeType (*)(MemoryPoolBase*)>> m_registered_pools;
};

HYP_API MemoryPoolManager& GetMemoryPoolManager()
{
    static MemoryPoolManager memory_pool_manager;

    return memory_pool_manager;
}

HYP_API void CalculateMemoryPoolUsage(Array<SizeType>& out_bytes_per_pool)
{
    GetMemoryPoolManager().CalculateMemoryUsage(out_bytes_per_pool);
}

#pragma endregion MemoryPoolManager

#pragma region MemoryPoolBase

MemoryPoolBase::MemoryPoolBase(ThreadID owner_thread_id, SizeType (*get_num_allocated_bytes)(MemoryPoolBase*))
    : m_owner_thread_id(owner_thread_id)
{
    GetMemoryPoolManager().RegisterPool(this, get_num_allocated_bytes);
}

MemoryPoolBase::~MemoryPoolBase()
{
    GetMemoryPoolManager().UnregisterPool(this);
}

#pragma endregion MemoryPoolBase

} // namespace memory
} // namespace hyperion