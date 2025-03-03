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

HYP_API IResourceMemoryPool *GetOrCreateResourceMemoryPool(TypeID type_id, UniquePtr<IResourceMemoryPool>(*create_fn)(void))
{
    Mutex::Guard guard(g_resource_memory_pools_mutex);

    auto it = g_resource_memory_pools.Find(type_id);

    if (it == g_resource_memory_pools.End()) {
        it = g_resource_memory_pools.Set(type_id, create_fn()).first;
    }

    return it->second.Get();
}

#pragma endregion Memory Pool

#pragma region ResourceBase

ResourceBase::ResourceBase()
    : m_is_initialized(false),
      m_ref_count(0),
      m_update_counter(0)
{
}

ResourceBase::ResourceBase(ResourceBase &&other) noexcept
    : m_is_initialized(other.m_is_initialized),
      m_ref_count(other.m_ref_count.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
      m_update_counter(other.m_update_counter.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
{
    other.m_is_initialized = false;
}

ResourceBase::~ResourceBase()
{
    // Ensure that the resources are no longer being used
    AssertThrowMsg(m_completion_semaphore.IsInSignalState(), "Resource destroyed while still in use, was WaitForCompletion() called?");
}

int ResourceBase::ClaimWithoutInitialize()
{
    return m_init_semaphore.Produce(1);
}

int ResourceBase::Claim()
{
    IThread *owner_thread = GetOwnerThread();

    auto Impl = [this]()
    {
        {
            HYP_NAMED_SCOPE("Initializing Resource - Initialization");

            // Wait for pre-init to complete
            m_pre_init_semaphore.Acquire();

            HYP_MT_CHECK_RW(m_data_race_detector);

            AssertThrow(!m_is_initialized);

            Initialize();
            m_is_initialized = true;
        }

        {
            HYP_NAMED_SCOPE("Initializing Resource - Post-Initialization Update");

            // Perform any updates that were requested before initialization
            int16 current_count = m_update_counter.Get(MemoryOrder::ACQUIRE);

            while (current_count != 0) {
                AssertThrow(current_count > 0);

                Update();

                current_count = m_update_counter.Decrement(current_count, MemoryOrder::ACQUIRE_RELEASE) - current_count;
            }
        }
    };

    HYP_SCOPE;

    return m_init_semaphore.Produce(1, [this, owner_thread, &Impl](bool is_signalled)
    {
        AssertThrow(is_signalled);

        m_completion_semaphore.Produce(1);

        if (!CanExecuteInline()) {
            EnqueueOp(std::move(Impl));
    
            return;
        }
    
        Impl();
    });
}

int ResourceBase::Unclaim()
{
    auto Impl = [this]()
    {
        HYP_NAMED_SCOPE("Destroying Resource");

        HYP_MT_CHECK_RW(m_data_race_detector);

        AssertThrow(m_is_initialized);

        Destroy();

        m_is_initialized = false;

        m_completion_semaphore.Release(1);
    };

    HYP_SCOPE;

    return m_init_semaphore.Release(1, [this, &Impl](bool is_signalled)
    {
        // Must be put into non-initialized state to destroy
        AssertThrow(!is_signalled);


        if (!CanExecuteInline()) {
            EnqueueOp(std::move(Impl));
    
            return;
        }
    
        Impl();
    });
}

void ResourceBase::SetNeedsUpdate()
{
    auto Impl = [this]()
    {
        HYP_NAMED_SCOPE("Applying Resource updates");

        int16 current_count = m_update_counter.Get(MemoryOrder::ACQUIRE);

        while (current_count != 0) {
            HYP_MT_CHECK_RW(m_data_race_detector);

            AssertThrow(current_count > 0);

            // Only should get here if already initialized due to the check when SetNeedsUpdate() is called
            AssertThrow(m_is_initialized);
            
            Update();

            current_count = m_update_counter.Decrement(current_count, MemoryOrder::ACQUIRE_RELEASE) - current_count;
        }

        m_completion_semaphore.Release(1);
    };

    HYP_SCOPE;

    m_completion_semaphore.Produce(1);

    // If not yet initialized, increment the counter and return immediately
    // Otherwise, we need to push a command to the owner thread to update the resources
    if (!m_init_semaphore.IsInSignalState()) {
        m_pre_init_semaphore.Produce(1);

        HYP_DEFER({
            m_pre_init_semaphore.Release(1);
        });

        // Check again, as it might have been initialized in between the initial check and the increment
        if (!m_init_semaphore.IsInSignalState()) {
            m_update_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

            m_completion_semaphore.Release(1);

            return;
        }
    }

    m_update_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

    if (!CanExecuteInline()) {
        EnqueueOp(std::move(Impl));

        return;
    }

    Impl();
}

void ResourceBase::WaitForCompletion()
{
    HYP_SCOPE;

    HYP_LOG(Resource, Debug, "Waiting for completion of Resource from thread {}", Threads::CurrentThreadID().GetName());

    if (CanExecuteInline()) {
        // Wait for any threads that are using this Resource pre-initialization to stop
        m_pre_init_semaphore.Acquire();

        // We need to flush pending tasks if we are on the owner thread and
        // we still have pending tasks. Not ideal, but we need to wrap up
        // destruction of resources before we can return.
        if (!m_completion_semaphore.IsInSignalState()) {
            HYP_NAMED_SCOPE("Flushing scheduler tasks");

            HYP_LOG(Resource, Debug, "Flushing scheduler tasks for Resource");
            
            FlushScheduledTasks();
        
            AssertThrow(m_completion_semaphore.IsInSignalState());
        }

        return;
    }

    // Wait for tasks to complete on another thread
    m_completion_semaphore.Acquire();
}

void ResourceBase::Execute(Proc<void> &&proc, bool force_owner_thread)
{
    HYP_SCOPE;

    m_completion_semaphore.Produce(1);

    if (!force_owner_thread) {
        if (!m_init_semaphore.IsInSignalState()) {
            // Initialization (happens on owner thread) will wait for this value to hit zero
            m_pre_init_semaphore.Produce(1);

            HYP_DEFER({
                m_pre_init_semaphore.Release(1);
            });

            // Check again, as it might have been initialized in between the initial check and the increment
            if (!m_init_semaphore.IsInSignalState()) {
                HYP_NAMED_SCOPE("Executing Resource Command - Inline");

                { // Critical section
                    HYP_MT_CHECK_RW(m_data_race_detector);

                    // Execute inline if not initialized yet instead of pushing to owner thread
                    proc();

                    m_completion_semaphore.Release(1);
                }

                return;
            }
        }
    }

    auto Impl = [this, proc = std::move(proc)]() mutable
    {
        HYP_NAMED_SCOPE("Executing Resource command");

        HYP_MT_CHECK_RW(m_data_race_detector);

        proc();

        m_completion_semaphore.Release(1);
    };

    if (!CanExecuteInline()) {
        EnqueueOp(std::move(Impl));

        return;
    }

    Impl();
}

bool ResourceBase::CanExecuteInline() const
{
    IThread *owner_thread = GetOwnerThread();

    return owner_thread == nullptr || Threads::IsOnThread(owner_thread->GetID());
}

void ResourceBase::FlushScheduledTasks()
{
    IThread *owner_thread = GetOwnerThread();
    AssertThrow(owner_thread != nullptr);

    owner_thread->GetScheduler().Flush([](auto &operation)
    {
        operation.Execute();
    });
}

void ResourceBase::EnqueueOp(Proc<void> &&proc)
{
    GetOwnerThread()->GetScheduler().Enqueue(
        std::move(proc),
        TaskEnqueueFlags::FIRE_AND_FORGET
    );
}

#pragma endregion ResourceBase

#pragma region NullResource

class NullResource final : public IResource
{
public:
    NullResource()                              = default;
    NullResource(NullResource &&other) noexcept = default;
    virtual ~NullResource() override            = default;

    virtual bool IsNull() const override
    {
        return true;
    }

    virtual int Claim() override
    {
        return 0;
    }

    virtual int Unclaim() override
    {
        return 0;
    }

    virtual void WaitForCompletion() override
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

HYP_API IResource &GetNullResource()
{
    static NullResource null_resource;

    return null_resource;
}

#pragma endregion NullResource

} // namespace hyperion