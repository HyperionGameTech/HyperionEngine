/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DELEGATE_HPP
#define HYPERION_DELEGATE_HPP

#include <core/functional/Proc.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/Name.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

namespace dotnet {
class Object;
} // namespace dotnet

namespace functional {

class IDelegate;

template <class ReturnType, class... Args>
class Delegate;

class DelegateHandler;

// Flag to set while deleting an entry - prevents read scopes from entering
// the critical section while the entry is potentially being deleted.

// In methods where multiple threads could attempt to acquire write access,
// such as adding new entries, we use a mutex to ensure exclusive access.
static constexpr uint64 g_write_flag = 0x1;

// A mask that is written when marking an entry for removal.
// An entry is marked for removal rather than being removed directly to limit the amount of exclusive locking required.

// When calling Broadcast(), delegate will also set this mask on a handler while executing the function that is assigned to the handler,
// preventing an entry from being deleted while it is executing (but still allowing other threads to MARK an entry for removal at a later time)
static constexpr uint64 g_read_mask = uint64(-1) & ~g_write_flag;

struct DelegateHandlerEntryBase
{
    uint32 index;
    AtomicVar<uint64> mask;
    ThreadID calling_thread_id;

    HYP_FORCE_INLINE void MarkForRemoval()
    {
        index = ~0u;
    }

    HYP_FORCE_INLINE bool IsMarkedForRemoval() const
    {
        return index == ~0u;
    }

    IThread* GetCallingThread() const
    {
        if (!calling_thread_id.IsValid())
        {
            return nullptr;
        }

        IThread* thread = Threads::GetThread(calling_thread_id);
        AssertThrow(thread != nullptr);

        return thread;
    }
};

template <class ProcType>
struct DelegateHandlerEntry : DelegateHandlerEntryBase
{
    ProcType proc;
};

struct DelegateHandler
{
    DelegateHandlerEntryBase* entry;
    void* delegate;
    void (*remove_fn)(void*, DelegateHandlerEntryBase*);
    void (*detach_fn)(void*, DelegateHandler&& delegate_handler);

    DelegateHandler()
        : entry(nullptr),
          delegate(nullptr),
          remove_fn(nullptr),
          detach_fn(nullptr)
    {
    }

    DelegateHandler(DelegateHandlerEntryBase* entry, void* delegate, void (*remove_fn)(void*, DelegateHandlerEntryBase*), void (*detach_fn)(void*, DelegateHandler&& delegate_handler))
        : entry(entry),
          delegate(delegate),
          remove_fn(remove_fn),
          detach_fn(detach_fn)
    {
    }

    DelegateHandler(const DelegateHandler& other) = delete;
    DelegateHandler& operator=(const DelegateHandler& other) = delete;

    DelegateHandler(DelegateHandler&& other) noexcept
        : entry(other.entry),
          delegate(other.delegate),
          remove_fn(other.remove_fn),
          detach_fn(other.detach_fn)
    {
        other.entry = nullptr;
        other.delegate = nullptr;
        other.remove_fn = nullptr;
        other.detach_fn = nullptr;
    }

    DelegateHandler& operator=(DelegateHandler&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        Reset();

        entry = other.entry;
        delegate = other.delegate;
        remove_fn = other.remove_fn;
        detach_fn = other.detach_fn;

        other.entry = nullptr;
        other.delegate = nullptr;
        other.remove_fn = nullptr;
        other.detach_fn = nullptr;

        return *this;
    }

    ~DelegateHandler()
    {
        Reset();
    }

    HYP_FORCE_INLINE void* GetDelegate() const
    {
        return delegate;
    }

    void Reset()
    {
        if (IsValid())
        {
            AssertThrow(remove_fn != nullptr);

            remove_fn(delegate, entry);
        }

        entry = nullptr;
        delegate = nullptr;
        remove_fn = nullptr;
        detach_fn = nullptr;
    }

    void Detach()
    {
        if (IsValid())
        {
            AssertThrow(detach_fn != nullptr);

            detach_fn(delegate, std::move(*this));
        }
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return entry != nullptr && delegate != nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const DelegateHandler& other) const
    {
        return entry == other.entry && delegate == other.delegate;
    }

    HYP_FORCE_INLINE bool operator!=(const DelegateHandler& other) const
    {
        return entry != other.entry || delegate != other.delegate;
    }

    HYP_FORCE_INLINE bool operator<(const DelegateHandler& other) const
    {
        if (entry != nullptr)
        {
            if (other.entry != nullptr)
            {
                return entry->index < other.entry->index;
            }

            return true;
        }

        return false;
    }
};

/*! \brief Stores a set of DelegateHandlers, intended to hold references to delegates and remove them upon destruction of the owner object. */
class DelegateHandlerSet : HashMap<Name, DelegateHandler, HashTable_DynamicNodeAllocator<KeyValuePair<Name, DelegateHandler>>>
{
public:
    using HashMap::ConstIterator;
    using HashMap::Iterator;

    HYP_FORCE_INLINE DelegateHandlerSet& Add(DelegateHandler&& delegate_handler)
    {
        HashMap::Insert({ Name::Unique("DelegateHandler_"), std::move(delegate_handler) });
        return *this;
    }

    HYP_FORCE_INLINE DelegateHandlerSet& Add(Name name, DelegateHandler&& delegate_handler)
    {
        HashMap::Insert({ name, std::move(delegate_handler) });
        return *this;
    }

    HYP_FORCE_INLINE bool Remove(WeakName name)
    {
        auto it = HashMap::FindAs(name);

        if (it == HashMap::End())
        {
            return false;
        }

        HashMap::Erase(it);

        return true;
    }

    HYP_FORCE_INLINE bool Remove(ConstIterator it)
    {
        if (it == HashMap::End())
        {
            return false;
        }

        HashMap::Erase(it);

        return true;
    }

    /*! \brief Remove all delegate handlers that are bound to the given \ref{delegate}
     *  \returns The number of delegate handlers that were removed. */
    template <class ReturnType, class... Args>
    HYP_FORCE_INLINE int Remove(Delegate<ReturnType, Args...>* delegate)
    {
        Array<DelegateHandler> delegate_handlers;

        for (auto it = HashMap::Begin(); it != HashMap::End();)
        {
            if (it->second.delegate == delegate)
            {
                delegate_handlers.PushBack(std::move(it->second));

                it = HashMap::Erase(it);

                continue;
            }

            ++it;
        }

        return int(delegate_handlers.Size());
    }

    HYP_FORCE_INLINE Iterator Find(WeakName name)
    {
        return HashMap::FindAs(name);
    }

    HYP_FORCE_INLINE ConstIterator Find(WeakName name) const
    {
        return HashMap::FindAs(name);
    }

    HYP_FORCE_INLINE bool Contains(WeakName name) const
    {
        return HashMap::FindAs(name) != HashMap::End();
    }

    HYP_DEF_STL_BEGIN_END(HashMap::Begin(), HashMap::End())
};

class IDelegate
{
public:
    virtual ~IDelegate() = default;

    virtual bool AnyBound() const = 0;

    virtual bool Remove(DelegateHandler&& handler) = 0;
    virtual int RemoveAllDetached() = 0;

protected:
    virtual bool Remove(DelegateHandlerEntryBase* entry) = 0;
};

/*! \brief A Delegate object that can be used to bind handler functions to be called when a broadcast is sent.
 *  Handlers can be bound as strong or weak references, and adding them is thread safe.
 *
 *  \tparam ReturnType The return type of the handler functions.
 *  \tparam Args The argument types of the handler functions. */
template <class ReturnType, class... Args>
class Delegate : public virtual IDelegate
{
public:
    using ProcType = Proc<ReturnType(Args...)>;

    Delegate()
        : m_num_procs(0),
          m_id_counter(0)
    {
    }

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;

    Delegate(Delegate&& other) noexcept
        : m_procs(std::move(other.m_procs)),
          m_num_procs(other.m_num_procs.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
          m_detached_handlers(std::move(other.m_detached_handlers)),
          m_id_counter(other.m_id_counter)
    {
        other.m_id_counter = 0;
    }

    Delegate& operator=(Delegate&& other) noexcept = delete;

    virtual ~Delegate() override
    {
        m_detached_handlers.Clear();

        for (auto it = m_procs.Begin(); it != m_procs.End(); ++it)
        {
            delete *it;
        }
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !Delegate::AnyBound();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return Delegate::AnyBound();
    }

    virtual bool AnyBound() const override final
    {
        return m_num_procs.Get(MemoryOrder::ACQUIRE) != 0;
    }

    /*! \brief Bind a Proc<> to the Delegate. The bound function will always be called on the thread that Bind() is called from if \ref{require_current_thread} is set to true.
     *  \note The handler will be removed when the last reference to the returned DelegateHandler is removed.
     *  This makes it easy to manage resource cleanup, as you can store the DelegateHandler as a class member and when the object is destroyed, the handler will be removed from the Delegate.
     *
     *  \param proc The Proc to bind.
     *  \param require_current_thread Should the delegate handler function be called on the current thread?
     *  \return  A reference counted DelegateHandler object that can be used to remove the handler from the Delegate. */
    HYP_NODISCARD DelegateHandler Bind(ProcType&& proc, bool require_current_thread = false)
    {
        AssertDebugMsg(std::is_void_v<ReturnType> || !require_current_thread, "Cannot use require_current_thread for non-void delegate return type");

        return Bind(std::move(proc), require_current_thread ? ThreadID::Current() : ThreadID::Invalid());
    }

    /*! \brief Bind a Proc<> to the Delegate.
     *  \note The handler will be removed when the last reference to the returned DelegateHandler is removed.
     *  This makes it easy to manage resource cleanup, as you can store the DelegateHandler as a class member and when the object is destroyed, the handler will be removed from the Delegate.
     *
     *  \param proc The Proc to bind.
     *  \param calling_thread_id The thread to call the bound function on.
     *  \return  A reference counted DelegateHandler object that can be used to remove the handler from the Delegate. */
    HYP_NODISCARD DelegateHandler Bind(ProcType&& proc, const ThreadID& calling_thread_id)
    {
        AssertDebugMsg(std::is_void_v<ReturnType> || !calling_thread_id.IsValid() || calling_thread_id == ThreadID::Current(), "Cannot call a handler on a different thread if the delegate returns a value");

        Mutex::Guard guard(m_mutex);

        DelegateHandlerEntry<ProcType>* entry = m_procs.PushBack(new DelegateHandlerEntry<ProcType>());
        entry->index = m_id_counter++;
        entry->calling_thread_id = calling_thread_id;
        entry->proc = std::move(proc);

        m_num_procs.Increment(1, MemoryOrder::RELEASE);

        return CreateDelegateHandler(entry);
    }

    /*! \brief Remove all detached handlers from the Delegate.
     *  \note Only detached handlers are removed, as removing bound handlers would cause them to hold dangling pointers.
     *  \return The number of handlers removed. */
    virtual int RemoveAllDetached() override
    {
        if (!AnyBound())
        {
            return 0;
        }

        Mutex::Guard guard(m_mutex);

        Mutex::Guard guard2(m_detached_handlers_mutex);
        m_detached_handlers.Clear();

        int num_removed = 0;

        for (auto it = m_procs.Begin(); it != m_procs.End();)
        {
            DelegateHandlerEntry<ProcType>* current = *it;

            // set write mask, loop until we have exclusive access.
            uint64 state = current->mask.BitOr(g_write_flag, MemoryOrder::ACQUIRE);
            while (state & g_read_mask)
            {
                state = current->mask.Get(MemoryOrder::ACQUIRE);
                Threads::Sleep(0);
            }

            if (current->IsMarkedForRemoval())
            {
                delete current;

                it = m_procs.Erase(it);

                ++num_removed;

                continue;
            }

            ++it;
        }

        m_num_procs.Decrement(uint32(num_removed), MemoryOrder::RELEASE);

        return num_removed;
    }

    virtual bool Remove(DelegateHandler&& handle) override
    {
        if (!handle.IsValid())
        {
            return false;
        }

        AssertDebug(handle.delegate == this);

        const bool remove_result = Remove(handle.entry);

        if (remove_result)
        {
            handle.delegate = nullptr;
            handle.entry = nullptr;
            handle.remove_fn = nullptr;
            handle.detach_fn = nullptr;

            return true;
        }

        return false;
    }

    /*! \brief Broadcast a message to all bound handlers.
     *  \note The default return value can be changed by specializing the \ref{hyperion::functional::ProcDefaultReturn} struct.
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref{ReturnType} if no handlers were bound. */
    template <class... ArgTypes>
    ReturnType Broadcast(ArgTypes&&... args)
    {
        if (!AnyBound())
        {
            // If no handlers are bound, return a default constructed object or void
            if constexpr (!std::is_void_v<ReturnType>)
            {
                return ProcDefaultReturn<ReturnType>::Get();
            }
            else
            {
                return;
            }
        }

        // Mutex to prevent adding new elements to the list or broadcasting from another thread.
        Mutex::Guard guard(m_mutex);

        const ThreadID current_thread_id = Threads::CurrentThreadID();

        ValueStorage<ReturnType> result_storage;
        bool result_constructed = false;

        for (auto it = m_procs.Begin(); it != m_procs.End();)
        {
            DelegateHandlerEntry<ProcType>* current = *it;

            // set write mask, loop until we have exclusive access.
            uint64 state = current->mask.BitOr(g_write_flag, MemoryOrder::ACQUIRE);
            while (state & g_read_mask)
            {
                state = current->mask.Get(MemoryOrder::ACQUIRE);
                Threads::Sleep(0);
            }

            if (current->IsMarkedForRemoval())
            {
                delete current;

                it = m_procs.Erase(it);

                m_num_procs.Decrement(1, MemoryOrder::RELEASE);

                continue;
            }

            // While we still have write access, mark the mask for reading, so we can prevent writes while calling
            current->mask.Increment(2, MemoryOrder::RELEASE);

            // Release write access
            current->mask.BitAnd(~g_write_flag, MemoryOrder::RELEASE);

            if constexpr (!std::is_void_v<ReturnType>)
            {
                AssertDebugMsg(!current->calling_thread_id.IsValid() || current->calling_thread_id == current_thread_id, "Cannot call a handler on a different thread if the delegate returns a value");

                if (result_constructed)
                {
                    result_storage.Destruct();
                }

                result_storage.Construct(current->proc(args...));

                // Check if object has been marked for removal by our call, and if so, release the proc's memory immediately.
                // The entry will be deleted and erased on next call to Broadcast() or RemoveAllDetached()
                if (current->IsMarkedForRemoval())
                {
                    current->proc.Reset();
                }

                // Release read access
                current->mask.Decrement(2, MemoryOrder::RELEASE);

                result_constructed = true;

                ++it;
            }
            else
            {
                if (current->calling_thread_id.IsValid() && current->calling_thread_id != current_thread_id)
                {
                    current->GetCallingThread()->GetScheduler().Enqueue([current, args_tuple = Tuple<ArgTypes...>(args...)]()
                        {
                            Apply([&proc = current->proc]<class... OtherArgs>(OtherArgs&&... args)
                                {
                                    proc(std::forward<OtherArgs>(args)...);
                                },
                                std::move(args_tuple));

                            if (current->IsMarkedForRemoval())
                            {
                                current->proc.Reset();
                            }

                            // Done reading
                            current->mask.Decrement(2, MemoryOrder::RELEASE);
                        },
                        TaskEnqueueFlags::FIRE_AND_FORGET);
                }
                else
                {
                    current->proc(args...);

                    if (current->IsMarkedForRemoval())
                    {
                        current->proc.Reset();
                    }

                    // Done reading
                    current->mask.Decrement(2, MemoryOrder::RELEASE);
                }

                ++it;
            }
        }

        if constexpr (!std::is_void_v<ReturnType>)
        {
            if (!result_constructed)
            {
                // If no handlers were called (due to elements being removed), return a default constructed object
                return ProcDefaultReturn<ReturnType>::Get();
            }

            ReturnType result = std::move(result_storage).Get();
            result_storage.Destruct();

            return result;
        }
    }

    /*! \brief Call operator overload - alias method for Broadcast().
     *  \note The default return value can be changed by specializing the \ref{hyperion::functional::ProcDefaultReturn} struct.
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref{ReturnType} if no handlers were bound. */
    template <class... ArgTypes>
    HYP_FORCE_INLINE ReturnType operator()(ArgTypes&&... args) const
    {
        return const_cast<Delegate*>(this)->Broadcast(std::forward<ArgTypes>(args)...);
    }

protected:
    virtual bool Remove(DelegateHandlerEntryBase* entry) override
    {
        if (!entry)
        {
            return false;
        }

        uint64 state;
        while (((state = entry->mask.Increment(2, MemoryOrder::ACQUIRE)) & g_write_flag))
        {
            entry->mask.Decrement(2, MemoryOrder::RELAXED);
            // wait for write flag to be released
            Threads::Sleep(0);
        }

        entry->MarkForRemoval();

        entry->mask.Decrement(2, MemoryOrder::RELEASE);

        return true;
    }

    static void RemoveDelegateHandlerCallback(void* delegate, DelegateHandlerEntryBase* entry)
    {
        Delegate* delegate_casted = static_cast<Delegate*>(delegate);

        delegate_casted->Remove(entry);
    }

    static void DetachDelegateHandlerCallback(void* delegate, DelegateHandler&& handler)
    {
        Delegate* delegate_casted = static_cast<Delegate*>(delegate);

        delegate_casted->DetachDelegateHandler(std::move(handler));
    }

    /*! \brief Add a delegate handler to hang around after its DelegateHandler is destructed */
    void DetachDelegateHandler(DelegateHandler&& handler)
    {
        Mutex::Guard guard(m_detached_handlers_mutex);

        m_detached_handlers.PushBack(std::move(handler));
    }

    DelegateHandler CreateDelegateHandler(DelegateHandlerEntry<ProcType>* entry)
    {
        return DelegateHandler {
            entry,
            static_cast<void*>(this),
            RemoveDelegateHandlerCallback,
            DetachDelegateHandlerCallback
        };
    }

    Array<DelegateHandlerEntry<ProcType>*, DynamicAllocator> m_procs;
    Array<DelegateHandler, DynamicAllocator> m_detached_handlers;
    Mutex m_detached_handlers_mutex;
    Mutex m_mutex;
    AtomicVar<uint32> m_num_procs;
    uint32 m_id_counter;
};

} // namespace functional

using functional::Delegate;
using functional::DelegateHandler;
using functional::DelegateHandlerSet;
using functional::IDelegate;

} // namespace hyperion

#endif