#include <core/memory/resource/Resource.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/profiling/ProfileScope.hpp>
#include <core/profiling/PerformanceClock.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Memory);
HYP_DEFINE_LOG_SUBCHANNEL(Resource, Memory);

#pragma region Memory Pool

static TypeMap<UniquePtr<IResourceMemoryPool>> s_resourceMemoryPools;
static Mutex s_resourceMemoryPoolsMutex;

IResourceMemoryPool* GetOrCreateResourceMemoryPool(TypeId typeId, UniquePtr<IResourceMemoryPool> (*createFn)(void))
{
    Mutex::Guard guard(s_resourceMemoryPoolsMutex);

    auto it = s_resourceMemoryPools.Find(typeId);

    if (it == s_resourceMemoryPools.End())
    {
        it = s_resourceMemoryPools.Set(typeId, createFn()).first;
    }

    return it->second.Get();
}

#pragma endregion Memory Pool

#pragma region ResourceBase

ResourceBase::ResourceBase()
    : m_refCount(0),
      m_isAlive(0)
{
}

ResourceBase::~ResourceBase()
{
    // Ensure that the resources are no longer being used
    HYP_CORE_ASSERT(m_refCount == 0, "Resource destroyed while still in use, was WaitForFinalization() called?");
}

bool ResourceBase::IsInitialized() const
{
    return 0 < AtomicAdd(&m_isAlive, 0);
}

int ResourceBase::IncRef()
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    if (++m_refCount == 1)
    {

        if (AtomicAdd(&m_isAlive, 0) == 0)
        {
            HYP_NAMED_SCOPE("Initializing Resource - Initialization");

            Initialize();

            AtomicExchange(&m_isAlive, 1);

            m_isAliveCV.NotifyAll();
        }
    }
    else
    {
        // Ensure its alive if we're not the one initializing
        while (AtomicAdd(&m_isAlive, 0) == 0)
        {
            m_isAliveCV.Wait(m_mutex);
        }
    }

    return m_refCount;
}

int ResourceBase::DecRef()
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    if (--m_refCount == 0)
    {
        int32 expected = 1;
        if (AtomicCompareExchange(&m_isAlive, expected, 0))
        {
            HYP_NAMED_SCOPE("Destroying Resource");

            Destroy();

            m_isAliveCV.NotifyAll();
        }
    }

    if (HYP_UNLIKELY(m_refCount < 0))
    {
        HYP_LOG(Resource, Fatal, "Resource ref count is negative! This is a bug in the code that uses this resource, please report it.\n\t"
                                 "Resource ref count: {}, address: {}",
            m_refCount, (void*)this);
    }

    return m_refCount;
}

void ResourceBase::WaitForFinalization() const
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    while (AtomicAdd(&m_isAlive, 0))
    {
        m_isAliveCV.Wait(m_mutex);
    }
}

#pragma endregion ResourceBase

#pragma region NullResource

class NullResource final : public IResource
{
public:
    NullResource() = default;
    NullResource(NullResource&& other) noexcept = default;
    virtual ~NullResource() override = default;

    virtual bool IsNull() const override
    {
        return true;
    }

    virtual int IncRef() override
    {
        return 0;
    }

    virtual int DecRef() override
    {
        return 0;
    }

    virtual void WaitForFinalization() const override
    {
        // Do nothing
    }
};

HYP_API IResource& GetNullResource()
{
    static NullResource s_nullResource;
    return s_nullResource;
}

#pragma endregion NullResource

} // namespace Hyperion
