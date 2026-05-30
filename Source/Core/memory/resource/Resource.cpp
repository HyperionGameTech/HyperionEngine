#include <Core/memory/resource/Resource.hpp>

#include <Core/containers/TypeMap.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/threading/Threads.hpp>
#include <Core/threading/Scheduler.hpp>

#include <Core/profiling/ProfileScope.hpp>
#include <Core/profiling/PerformanceClock.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/logging/Logger.hpp>

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Memory);
CORE_API HYP_DEFINE_LOG_SUBCHANNEL(Resource, Memory);

#pragma region ResourceGuard

ResourceGuard::ResourceGuard(ResourceBase& resource, int mask)
    : resource(&resource),
      mask(mask)
{
    if (mask & Write)
    {
        resource.AddWriter();
    }
    else
    {
        resource.AddReader();
    }
}

ResourceGuard::ResourceGuard(const ResourceGuard& other)
    : resource(other.resource),
      mask(other.mask)
{
    if (resource)
    {
        if (mask & Write)
        {
            resource->AddWriter();
        }
        else
        {
            resource->AddReader();
        }
    }
}

ResourceGuard& ResourceGuard::operator=(const ResourceGuard& other)
{
    if (this == &other)
        return *this;

    Release();

    AssertDebug(!(other.mask & Write), "Would cause deadlock!");

    resource = other.resource;
    mask = other.mask;

    if (resource != nullptr)
    {
        if (mask & Write)
        {
            resource->AddWriter();
        }
        else
        {
            resource->AddReader();
        }
    }

    return *this;
}

ResourceGuard::ResourceGuard(ResourceGuard&& other) noexcept
    : resource(other.resource),
      mask(other.mask)
{
    other.resource = nullptr;
}

ResourceGuard& ResourceGuard::operator=(ResourceGuard&& other) noexcept
{
    Release();

    resource = other.resource;
    mask = other.mask;

    other.resource = nullptr;

    return *this;
}

ResourceGuard::~ResourceGuard()
{
    Release();
}

void ResourceGuard::Release()
{
    if (resource != nullptr)
    {
        if (mask & Write)
        {
            resource->ReleaseWriter();
        }
        else
        {
            resource->ReleaseReader();
        }

        resource = nullptr;
    }
}

#pragma endregion ResourceGuard

#pragma region ResourceBase

ResourceBase::ResourceBase()
    : m_state(0),
      m_isInitialized(false)
{
}

ResourceBase::~ResourceBase()
{
    HYP_NAMED_SCOPE("Wait for readers to finish with resource");

    // calling AddWriter() here waits for all reads to complete and
    // blocks new readers/writers from acquiring the resource while we're destroying it.
    AddWriter(/* doInitialize */ false);
}

ResourceGuard ResourceBase::GetWriteScope()
{
    return ResourceGuard { *this, ResourceGuard::Write };
}

ResourceGuard ResourceBase::GetReadScope()
{
    return ResourceGuard { *this, ResourceGuard::Read };
}

void ResourceBase::AddWriter(bool doInitialize)
{
    uint32 numSpins = 0;

    int64 expected = 0;
    while (!AtomicCompareExchange(&m_state, expected, 1))
    {
        expected = 0;

        // volatile read
        while (m_state != 0)
        {
            if (numSpins++ < 16)
            {
                HYP_WAIT_IDLE();
            }
            else
            {
                // yield to other threads
                ThreadSleep(0);
            }
        }
    }

    if (doInitialize)
    {
        Mutex::Guard initGuard(m_initMutex);

        Initialize();

        m_isInitialized.Store(true);
    }
}

void ResourceBase::ReleaseWriter(bool doDeinitialize)
{
    if (doDeinitialize)
    {
        Mutex::Guard initGuard(m_initMutex);

        Assert(m_isInitialized.LoadVolatile());

        Destroy();

        m_isInitialized.Store(false);
    }

    AtomicBitAnd(&m_state, ~0x1);
}

void ResourceBase::AddReader()
{
    uint32 numSpins = 0;

    union
    {
        int64 state;
        uint64 ustate;
    };

    auto MaybeInitialize = [this](int64 state)
    {
        bool isInitializedLocal = false;

        if (state == 0)
        {
            bool shouldNotifyWaiters = false;

            {
                // successfully acquired read lock
                Mutex::Guard initGuard(m_initMutex);

                isInitializedLocal = m_isInitialized.Load();

                if (!isInitializedLocal)
                {
                    // need to do initialize here, since we're the first reader
                    m_isInitialized.Store(true);
                    isInitializedLocal = true;

                    Initialize();

                    shouldNotifyWaiters = true;
                }
            }

            if (shouldNotifyWaiters)
            {
                m_initCV.NotifyAll();
            }
        }

        if (!isInitializedLocal)
        {
            // wait for initialization to complete
            while (!m_isInitialized.Load())
            {
                Mutex::Guard initGuard(m_initMutex);

                m_initCV.Wait(m_initMutex);
            }
        }
    };

    // first pass: optimistic read
    if ((m_state & 0x1) == 0)
    {
        state = AtomicAdd(&m_state, 2);

        if ((state & 0x1) == 0)
        {
            MaybeInitialize(state);

            return;
        }

        AtomicSub(&m_state, 2);
    }

    while (true)
    {
        // failed, wait for writer to release
        if (m_state & 0x1)
        {
            if (numSpins++ < 16)
            {
                HYP_WAIT_IDLE();
            }
            else
            {
                ThreadSleep(0);
            }

            continue;
        }

        state = AtomicAdd(&m_state, 2);

        if ((state & 0x1) == 0)
        {
            MaybeInitialize(state);

            return;
        }

        AtomicSub(&m_state, 2);
    }
}

void ResourceBase::ReleaseReader()
{
    Mutex::Guard initGuard(m_initMutex);

    if (m_isInitialized.Load() && AtomicSub(&m_state, 2) == 2)
    {
        Destroy();

        m_isInitialized.Store(false);
    }
}

void ResourceBase::GetNumUsers(int64& outReaders, int64& outWriters) const
{
    int64 state = AtomicAdd(const_cast<volatile int64*>(&m_state), 0);

    outReaders = state >> 1;
    outWriters = state & 0x1;
}

#pragma endregion ResourceBase

} // namespace Hyperion
