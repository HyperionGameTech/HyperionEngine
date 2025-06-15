#include <core/memory/resource/Resource.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Memory);
HYP_DEFINE_LOG_SUBCHANNEL(Resource, Memory);

#pragma region Memory Pool

static TypeMap<UniquePtr<IResourceMemoryPool>> g_resource_memory_pools;
static Mutex g_resource_memory_pools_mutex;

HYP_API IResourceMemoryPool* GetOrCreateResourceMemoryPool(TypeID type_id, UniquePtr<IResourceMemoryPool> (*create_fn)(void))
{
    Mutex::Guard guard(g_resource_memory_pools_mutex);

    auto it = g_resource_memory_pools.Find(type_id);

    if (it == g_resource_memory_pools.End())
    {
        it = g_resource_memory_pools.Set(type_id, create_fn()).first;
    }

    return it->second.Get();
}

#pragma endregion Memory Pool

#pragma region ResourceBase

ResourceBase::ResourceBase()
    : m_initialization_mask(0),
      m_initialization_thread_id(ThreadID::Invalid()),
      m_update_counter(0)
{
}

ResourceBase::ResourceBase(ResourceBase&& other) noexcept
    : m_initialization_mask(other.m_initialization_mask.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
      m_initialization_thread_id(other.m_initialization_thread_id),
      m_update_counter(other.m_update_counter.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
{
    other.m_initialization_thread_id = ThreadID::Invalid();
}

ResourceBase::~ResourceBase()
{
    // Ensure that the resources are no longer being used
    AssertThrowMsg(m_ref_counter.IsInSignalState(), "Resource destroyed while still in use, was WaitForFinalization() called?");
}

bool ResourceBase::IsInitialized() const
{
    return m_initialization_mask.Get(MemoryOrder::ACQUIRE) & g_initialization_mask_initialized_bit;
}

int ResourceBase::IncRefNoInitialize()
{
    int count = m_ref_counter.Produce(1);

    if (count == 1)
    {
        // loop until we have exclusive access.
        uint64 state = m_initialization_mask.BitOr(g_initialization_mask_initialized_bit, MemoryOrder::ACQUIRE);
        AssertDebugMsg(!(state & g_initialization_mask_initialized_bit), "Attempted to initialize a resource that is already initialized");

        while (state & g_initialization_mask_read_mask)
        {
            state = m_initialization_mask.Get(MemoryOrder::ACQUIRE);
            Threads::Sleep(0);
        }

        m_initialization_thread_id = Threads::CurrentThreadID();
    }

    return count;
}

int ResourceBase::IncRef()
{
    ThreadBase* owner_thread = GetOwnerThread();

    auto impl = [this]()
    {
        {
            HYP_NAMED_SCOPE("Initializing Resource - Initialization");

            uint64 state = m_initialization_mask.BitOr(g_initialization_mask_initialized_bit, MemoryOrder::ACQUIRE_RELEASE);
            AssertDebugMsg(!(state & g_initialization_mask_initialized_bit), "Attempted to initialize a resource that is already initialized! Initialization thread id: %s (%u)",
                m_initialization_thread_id.GetName().LookupString(), m_initialization_thread_id.GetValue());

            // loop until we have exclusive access.
            while (state & g_initialization_mask_read_mask)
            {
                state = m_initialization_mask.Get(MemoryOrder::ACQUIRE);
                Threads::Sleep(0);
            }

            HYP_MT_CHECK_RW(m_data_race_detector);

            m_initialization_thread_id = Threads::CurrentThreadID();

            Initialize();
        }

        {
            HYP_NAMED_SCOPE("Initializing Resource - Post-Initialization Update");

            // Perform any updates that were requested before initialization
            int16 current_update_count = m_update_counter.Get(MemoryOrder::ACQUIRE);

            while (current_update_count != 0)
            {
                AssertThrow(current_update_count > 0);

                Update();

                current_update_count = m_update_counter.Decrement(current_update_count, MemoryOrder::ACQUIRE_RELEASE) - current_update_count;
            }
        }

        m_completion_semaphore.Release(1);
    };

    HYP_SCOPE;

    m_completion_semaphore.Produce(1);
    bool should_release = true;

    int result = m_ref_counter.Produce(1);

    if (result == 1)
    {
        should_release = false;

        if (CanExecuteInline())
        {
            impl();
        }
        else
        {
            EnqueueOp(std::move(impl));
        }
    }

    if (should_release)
    {
        m_completion_semaphore.Release(1);
    }

    return result;
}

int ResourceBase::DecRef()
{
    auto impl = [this]()
    {
        HYP_NAMED_SCOPE("Destroying Resource");

        uint64 state = m_initialization_mask.BitOr(g_initialization_mask_initialized_bit, MemoryOrder::ACQUIRE_RELEASE);
        AssertDebugMsg(state & g_initialization_mask_initialized_bit, "ResourceBase::DecRef() called on a resource that is not initialized!");

        // Wait till all reads are complete before we continue
        while (state & g_initialization_mask_read_mask)
        {
            state = m_initialization_mask.Get(MemoryOrder::ACQUIRE);
            Threads::Sleep(0);
        }

        // While we still have the initialized bit set, add the read access, so Initialize doesn't happen while we are destroying
        m_initialization_mask.Increment(2, MemoryOrder::RELEASE);
        // Unset the initialized bit
        m_initialization_mask.BitAnd(~g_initialization_mask_initialized_bit, MemoryOrder::RELEASE);

        // DEBUGGING
        state = m_initialization_mask.Get(MemoryOrder::ACQUIRE);
        AssertDebugMsg(!(state & g_initialization_mask_initialized_bit), "ResourceBase::DecRef() called on a resource that is still initialized! Initialization thread id: %s (%u)",
            m_initialization_thread_id.GetName().LookupString(), m_initialization_thread_id.GetValue());

        {
            HYP_MT_CHECK_RW(m_data_race_detector);

            Destroy();

            m_initialization_thread_id = ThreadID::Invalid();
        }

        // Done with destroying, we can now remove our read access
        m_initialization_mask.Decrement(2, MemoryOrder::RELEASE);

        m_completion_semaphore.Release(1);
    };

    HYP_SCOPE;

    m_completion_semaphore.Produce(1);
    bool should_release = true;

    int result = m_ref_counter.Release(1);

    if (result == 0)
    {
        should_release = false;

        if (CanExecuteInline())
        {

            impl();
        }
        else
        {
            EnqueueOp(std::move(impl));
        }
    }

    if (should_release)
    {
        m_completion_semaphore.Release(1);
    }

    return result;
}

void ResourceBase::SetNeedsUpdate()
{
    auto impl = [this]()
    {
        HYP_NAMED_SCOPE("Applying Resource updates");

        {
            HYP_MT_CHECK_READ(m_data_race_detector);

            int16 current_count = m_update_counter.Get(MemoryOrder::ACQUIRE);

            while (current_count != 0)
            {
                AssertDebug(current_count > 0);

                HYP_MT_CHECK_RW(m_data_race_detector);

                Update();

                current_count = m_update_counter.Decrement(current_count, MemoryOrder::ACQUIRE_RELEASE) - current_count;
            }
        }

        m_completion_semaphore.Release(1);
    };

    HYP_SCOPE;

    m_completion_semaphore.Produce(1);

    if (m_ref_counter.IsInSignalState())
    {
        // Check initialization state -- if it is not initialized, we increment the update counter
        // without waiting for the owner thread to finish
        if (!(m_initialization_mask.Increment(2, MemoryOrder::ACQUIRE) & g_initialization_mask_initialized_bit))
        {
            m_update_counter.Increment(1, MemoryOrder::RELEASE);

            // Remove our read access
            m_initialization_mask.Decrement(2, MemoryOrder::RELAXED);

            m_completion_semaphore.Release(1);

            // No work to be done yet since we aren't initialized.
            return;
        }

        // Remove our read access
        m_initialization_mask.Decrement(2, MemoryOrder::RELAXED);
    }

    m_update_counter.Increment(1, MemoryOrder::RELEASE);

    if (!CanExecuteInline())
    {
        EnqueueOp(std::move(impl));

        return;
    }

    impl();
}

void ResourceBase::WaitForTaskCompletion() const
{
    HYP_SCOPE;

    if (CanExecuteInline())
    {
        if (!m_completion_semaphore.IsInSignalState())
        {
            if (GetOwnerThread() == nullptr)
            { // No owner thread is used to execute tasks on.
                HYP_FAIL("No owner thread - cannot wait for task completion");
            }
            else
            { // We are on the owner thread
                // We need to flush pending tasks if we are on the owner thread and
                // we still have pending tasks.
                HYP_NAMED_SCOPE("Flushing scheduler tasks");

                HYP_LOG(Resource, Debug, "Flushing scheduler tasks for Resource");

                FlushScheduledTasks();
            }

            AssertThrow(m_completion_semaphore.IsInSignalState());
        }

        return;
    }

    // Wait for tasks to complete on another thread
    m_completion_semaphore.Acquire();
}

void ResourceBase::WaitForFinalization() const
{
    HYP_SCOPE;

    WaitForTaskCompletion();

    m_ref_counter.Acquire();
}

bool ResourceBase::CanExecuteInline() const
{
    ThreadBase* owner_thread = GetOwnerThread();

    return owner_thread == nullptr || Threads::IsOnThread(owner_thread->GetID());
}

void ResourceBase::FlushScheduledTasks() const
{
    ThreadBase* owner_thread = GetOwnerThread();
    AssertThrow(owner_thread != nullptr);

    owner_thread->GetScheduler().Flush([](auto& operation)
        {
            operation.Execute();
        });
}

void ResourceBase::EnqueueOp(Proc<void()>&& proc)
{
    GetOwnerThread()->GetScheduler().Enqueue(std::move(proc), TaskEnqueueFlags::FIRE_AND_FORGET);
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

    virtual int IncRefNoInitialize() override
    {
        return 0;
    }

    virtual int DecRef() override
    {
        return 0;
    }

    virtual void WaitForTaskCompletion() const override
    {
        // Do nothing
    }

    virtual void WaitForFinalization() const override
    {
        // Do nothing
    }

    virtual ResourceMemoryPoolHandle GetPoolHandle() const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual void SetPoolHandle(ResourceMemoryPoolHandle) override
    {
        HYP_NOT_IMPLEMENTED_VOID();
    }
};

HYP_API IResource& GetNullResource()
{
    static NullResource null_resource;

    return null_resource;
}

#pragma endregion NullResource

} // namespace hyperion