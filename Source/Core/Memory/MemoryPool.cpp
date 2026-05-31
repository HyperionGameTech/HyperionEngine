/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Memory/MemoryPool.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Containers/Array.hpp>

namespace Hyperion {
namespace memory {

#pragma region MemoryPoolManager

class MemoryPoolManager
{
public:
    void RegisterPool(MemoryPoolBase* pool, size_t (*getNumAllocatedBytes)(MemoryPoolBase*))
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

    void CalculateMemoryUsage(Array<Pair<MemoryPoolBase*, size_t>>& outBytesPerPool)
    {
        Mutex::Guard guard(m_mutex);

        outBytesPerPool.Reserve(m_registeredPools.Size());

        for (size_t i = 0; i < m_registeredPools.Size(); i++)
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
    Array<Pair<MemoryPoolBase*, size_t (*)(MemoryPoolBase*)>> m_registeredPools;
};

CORE_API MemoryPoolManager& GetMemoryPoolManager()
{
    static MemoryPoolManager memoryPoolManager;

    return memoryPoolManager;
}

CORE_API void CalculateMemoryUsagePerPool(Array<Pair<MemoryPoolBase*, size_t>>& outBytesPerPool)
{
    GetMemoryPoolManager().CalculateMemoryUsage(outBytesPerPool);
}

#pragma endregion MemoryPoolManager

#pragma region MemoryPoolBase

MemoryPoolBase::MemoryPoolBase(Name poolName, ThreadId ownerThreadId, size_t (*getNumAllocatedBytes)(MemoryPoolBase*))
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
