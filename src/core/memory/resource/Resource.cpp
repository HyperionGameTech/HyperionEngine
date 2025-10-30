#include <core/memory/resource/Resource.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/profiling/ProfileScope.hpp>
#include <core/profiling/PerformanceClock.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

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
      m_initState(0)
{
}

ResourceBase::~ResourceBase()
{
    // Ensure that the resources are no longer being used
    HYP_CORE_ASSERT(m_refCount == 0, "Resource destroyed while still in use, was WaitForFinalization() called?");
}

bool ResourceBase::IsInitialized() const
{
    return m_initState.GetValue();
}

int ResourceBase::IncRef()
{
    HYP_SCOPE;

    int result = AtomicIncrement(&m_refCount);

    if (result == 1)
    {
        Mutex::Guard guard(m_mutex);

        m_initState.Produce(1, [&](bool)
            {
                HYP_NAMED_SCOPE("Initializing Resource - Initialization");
                HYP_MT_CHECK_RW(m_dataRaceDetector);

                Initialize();
            });
    }

    return result;
}

int ResourceBase::DecRef()
{
    HYP_SCOPE;

    int result = AtomicDecrement(&m_refCount);

    if (result == 0)
    {
        Mutex::Guard guard(m_mutex);

        if (!m_initState.GetValue())
        {
            return result;
        }

        HYP_NAMED_SCOPE("Destroying Resource");
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        Destroy();

        m_initState.Release();
    }

    if (HYP_UNLIKELY(result < 0))
    {
        HYP_LOG(Resource, Fatal, "Resource ref count is negative! This is a bug in the code that uses this resource, please report it.\n\t"
                                 "Resource ref count: {}, address: {}",
            result, (void*)this);
    }

    return result;
}

void ResourceBase::WaitForFinalization() const
{
    HYP_SCOPE;

    m_initState.Acquire();

    if (HYP_UNLIKELY(m_refCount != 0))
    {
        PerformanceClock timer;
        timer.Start();

        do
        {
            Threads::Sleep(0);
        }
        while (m_refCount != 0 && timer.ElapsedMs() < 30.0);

        if (m_refCount != 0)
        {
            HYP_LOG(Resource, Fatal, "Resource could not be finalized; must be locked elsewhere! "
                                     "This is a bug in the code that uses this resource, please report it.\n\t"
                                     "Resource ref count: {}, address: {}",
                m_refCount, (void*)this);
        }
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

    virtual int NumRefs() const override
    {
        return 0;
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
    static NullResource nullResource;

    return nullResource;
}

#pragma endregion NullResource

} // namespace hyperion
