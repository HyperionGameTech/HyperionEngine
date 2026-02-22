/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/functional/Proc.hpp>

#include <Core/containers/Array.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/threading/SharedMutex.hpp>
#include <Core/threading/Threads.hpp>
#include <Core/threading/Task.hpp>
#include <Core/threading/Scheduler.hpp>

#include <Core/utilities/ForEach.hpp>
#include <Core/utilities/DeferredScope.hpp>

#include <Core/Name.hpp>

#include <Core/Defines.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

namespace threading {

class ThreadId;
class ThreadBase;

HYP_API extern void ThreadSleep(uint32 milliseconds);
HYP_API extern ThreadBase* GetThreadById(const ThreadId& threadId);

} // namespace threading

using threading::GetThreadById;
using threading::ThreadBase;
using threading::ThreadSleep;

namespace functional {

class IDelegate;

template <class ReturnType, class... Args>
class Delegate;

class DelegateHandler;

// Flag to set while deleting an entry - prevents read scopes from entering
// the critical section while the entry is potentially being deleted.

// In methods where multiple threads could attempt to acquire write access,
// such as adding new entries, we use a mutex to ensure exclusive access.
static constexpr uint64 ExclusiveAccessFlag = 0x1;

// A mask that is written when marking an entry for removal.
// An entry is marked for removal rather than being removed directly to limit the amount of exclusive locking required.

// When calling Broadcast(), delegate will also set this mask on a handler while executing the function that is assigned to the handler,
// preventing an entry from being deleted while it is executing (but still allowing other threads to MARK an entry for removal at a later time)
static constexpr uint64 SharedAccessMask = uint64(-1) & ~ExclusiveAccessFlag;

struct DelegateHandlerEntryBase
{
    uint32 index;
    AtomicVar<uint64> mask;
    ThreadId callingThreadId;

    ~DelegateHandlerEntryBase()
    {
        while (HYP_UNLIKELY(mask.Get(MemoryOrder::ACQUIRE) & SharedAccessMask))
        {
            HYP_NAMED_SCOPE("~DelegateHandlerEntryBase() - Waiting for read scopes to finish");
            ThreadSleep(0);
        }
    }

    HYP_FORCE_INLINE void MarkForRemoval()
    {
        index = ~0u;
    }

    HYP_FORCE_INLINE bool IsMarkedForRemoval() const
    {
        return index == ~0u;
    }

    ThreadBase* GetCallingThread() const
    {
        if (!callingThreadId.IsValid())
        {
            return nullptr;
        }

        ThreadBase* thread = GetThreadById(callingThreadId);
        HYP_CORE_ASSERT(thread != nullptr);

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
    void* delegateImpl;
    void (*removeFn)(void*, DelegateHandlerEntryBase*);
    void (*detachFn)(void*, DelegateHandler&& delegateHandler);

    DelegateHandler()
        : entry(nullptr),
          delegateImpl(nullptr),
          removeFn(nullptr),
          detachFn(nullptr)
    {
    }

    DelegateHandler(DelegateHandlerEntryBase* entry, void* delegateImpl, void (*removeFn)(void*, DelegateHandlerEntryBase*), void (*detachFn)(void*, DelegateHandler&& delegateHandler))
        : entry(entry),
          delegateImpl(delegateImpl),
          removeFn(removeFn),
          detachFn(detachFn)
    {
    }

    DelegateHandler(const DelegateHandler& other) = delete;
    DelegateHandler& operator=(const DelegateHandler& other) = delete;

    DelegateHandler(DelegateHandler&& other) noexcept
        : entry(other.entry),
          delegateImpl(other.delegateImpl),
          removeFn(other.removeFn),
          detachFn(other.detachFn)
    {
        other.entry = nullptr;
        other.delegateImpl = nullptr;
        other.removeFn = nullptr;
        other.detachFn = nullptr;
    }

    DelegateHandler& operator=(DelegateHandler&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        Reset();

        entry = other.entry;
        delegateImpl = other.delegateImpl;
        removeFn = other.removeFn;
        detachFn = other.detachFn;

        other.entry = nullptr;
        other.delegateImpl = nullptr;
        other.removeFn = nullptr;
        other.detachFn = nullptr;

        return *this;
    }

    ~DelegateHandler()
    {
        Reset();
    }

    HYP_FORCE_INLINE void* GetDelegate() const
    {
        return delegateImpl;
    }

    void Reset()
    {
        if (IsValid())
        {
            HYP_CORE_ASSERT(removeFn != nullptr);

            removeFn(delegateImpl, entry);
        }

        entry = nullptr;
        delegateImpl = nullptr;
        removeFn = nullptr;
        detachFn = nullptr;
    }

    void Detach()
    {
        if (IsValid())
        {
            HYP_CORE_ASSERT(detachFn != nullptr);

            detachFn(delegateImpl, std::move(*this));
        }
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return entry != nullptr && delegateImpl != nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const DelegateHandler& other) const
    {
        return entry == other.entry && delegateImpl == other.delegateImpl;
    }

    HYP_FORCE_INLINE bool operator!=(const DelegateHandler& other) const
    {
        return entry != other.entry || delegateImpl != other.delegateImpl;
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

class IDelegate
{
public:
    virtual ~IDelegate() = default;

    virtual bool AnyBound() const = 0;

    virtual bool Remove(DelegateHandler&& handler) = 0;
    virtual int RemoveAllDetached() = 0;
};

/*! \brief A Delegate object that can be used to bind handler functions to be called when a broadcast is sent.
 *  Handlers can be bound as strong or weak references, and adding them is thread safe.
 *  \tparam ReturnType The return type of the handler functions.
 *  \tparam Args The argument types of the handler functions. */
template <class ReturnType, class... Args>
class DelegateImpl final
{
    using ProcType = Proc<ReturnType(Args...)>;
    using ProcList = Array<DelegateHandlerEntry<ProcType>*>;

public:
    DelegateImpl()
        : m_numProcs(0),
          m_idCounter(0),
          m_activeIdx(0)
    {
    }

    DelegateImpl(const DelegateImpl& other) = delete;
    DelegateImpl& operator=(const DelegateImpl& other) = delete;

    DelegateImpl(DelegateImpl&& other) noexcept = delete;
    DelegateImpl& operator=(DelegateImpl&& other) noexcept = delete;

    ~DelegateImpl()
    {
        m_detachedHandlers.Clear();

        // should always end back on our first list
        AssertDebug(m_activeIdx % 2 == 0);
        AssertDebug(m_lists[1].Empty());

        for (DelegateHandlerEntry<ProcType>* entry : m_lists[0])
        {
            while (HYP_UNLIKELY(entry->mask.Get(MemoryOrder::ACQUIRE) & SharedAccessMask))
            {
                HYP_NAMED_SCOPE("~DelegateImpl() - Waiting for read scopes to finish");
                ThreadSleep(0);
            }

            delete entry;
        }
    }

    bool AnyBound() const
    {
        return m_numProcs.Get(MemoryOrder::ACQUIRE) != 0;
    }

    /*! \brief Bind a Proc<> to the Delegate. The bound function will always be called on the thread that Bind() is called from if \ref requireCurrentThread is set to true.
     *  \note The handler will be removed when the last reference to the returned DelegateHandler is removed.
     *  This makes it easy to manage resource cleanup, as you can store the DelegateHandler as a class member and when the object is destroyed, the handler will be removed from the Delegate.
     *
     *  \param proc The Proc to bind.
     *  \return  A reference counted DelegateHandler object that can be used to remove the handler from the Delegate. */
    HYP_NODISCARD DelegateHandler Bind(ProcType&& proc)
    {
        ProcList& list = LockActiveList();

        DelegateHandlerEntry<ProcType>* entry = list.PushBack(new DelegateHandlerEntry<ProcType>());
        entry->index = m_idCounter++;
        entry->callingThreadId = ThreadId::Invalid();
        entry->proc = std::move(proc);

        m_numProcs.Increment(1, MemoryOrder::RELEASE);

        DelegateHandler res = CreateDelegateHandler(entry);

        UnlockList(list);

        return res;
    }

    /*! \brief Bind a Proc<> to the Delegate.
     *  \note The handler will be removed when the last reference to the returned DelegateHandler is removed.
     *  This makes it easy to manage resource cleanup, as you can store the DelegateHandler as a class member and when the object is destroyed, the handler will be removed from the Delegate.
     *
     *  \param proc The Proc to bind.
     *  \param callingThreadId The thread to call the bound function on.
     *  \return  A reference counted DelegateHandler object that can be used to remove the handler from the Delegate. */
    HYP_NODISCARD DelegateHandler BindThreaded(ProcType&& proc, const ThreadId& callingThreadId)
    {
        HYP_CORE_ASSERT(std::is_void_v<ReturnType> || !callingThreadId.IsValid() || callingThreadId == ThreadId::Current(), "Cannot call a handler on a different thread if the delegate returns a value");

#ifdef HYP_DEBUG_MODE
        if (callingThreadId != ThreadId::Invalid())
        {
            HYP_CORE_ASSERT(GetThreadById(callingThreadId) != nullptr, "Cannot bind a handler to a thread that is not registered with the Threads system");
        }
#endif

        ProcList& list = LockActiveList();

        DelegateHandlerEntry<ProcType>* entry = list.PushBack(new DelegateHandlerEntry<ProcType>());
        entry->index = m_idCounter++;
        entry->callingThreadId = callingThreadId;
        entry->proc = std::move(proc);

        m_numProcs.Increment(1, MemoryOrder::RELEASE);

        DelegateHandler res = CreateDelegateHandler(entry);

        UnlockList(list);

        return res;
    }

    /*! \brief Remove all detached handlers from the Delegate.
     *  \note Only detached handlers are removed, as removing bound handlers would cause them to hold dangling pointers.
     *  \return The number of handlers removed. */
    int RemoveAllDetached()
    {
        if (!AnyBound())
        {
            return 0;
        }

        Mutex::Guard guard(m_detachedHandlersMutex);
        m_detachedHandlers.Clear();

        int numRemoved = 0;

        ProcList& list = LockList(0);

        for (auto it = list.Begin(); it != list.End();)
        {
            DelegateHandlerEntry<ProcType>* current = *it;

            // set write mask, loop until we have exclusive access.
            uint64 state = current->mask.BitOr(ExclusiveAccessFlag, MemoryOrder::ACQUIRE);
            while (state & SharedAccessMask)
            {
                state = current->mask.Get(MemoryOrder::ACQUIRE);
                HYP_WAIT_IDLE();
            }

            if (current->IsMarkedForRemoval())
            {
                delete current;

                it = list.Erase(it);

                ++numRemoved;

                continue;
            }
            
            // release write flag
            current->mask.BitAnd(~ExclusiveAccessFlag, MemoryOrder::RELEASE);

            ++it;
        }

        m_numProcs.Decrement(uint32(numRemoved), MemoryOrder::RELEASE);

        UnlockList(list);

        return numRemoved;
    }

    bool Remove(DelegateHandler&& handle)
    {
        if (!handle.IsValid())
        {
            return false;
        }

        HYP_CORE_ASSERT(handle.delegateImpl == this);

        const bool removeResult = Remove(handle.entry);

        if (removeResult)
        {
            handle.delegateImpl = nullptr;
            handle.entry = nullptr;
            handle.removeFn = nullptr;
            handle.detachFn = nullptr;

            return true;
        }

        return false;
    }

    /*! \brief Broadcast a message to all bound handlers.
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref ReturnType if no handlers were bound. */
    template <class... ArgTypes>
    ReturnType Broadcast(ArgTypes&&... args)
    {
        if (!AnyBound())
        {
            // If no handlers are bound, return a default constructed object or void
            if constexpr (!std::is_void_v<ReturnType>)
            {
                return ReturnType();
            }
            else
            {
                return;
            }
        }

        const ThreadId currentThreadId = CurrentThreadId();

        ValueStorage<ReturnType> resultStorage;
        bool resultConstructed = false;

        ProcList& list = LockList(0);
        SwapActiveLists();

        for (auto it = list.Begin(); it != list.End();)
        {
            DelegateHandlerEntry<ProcType>* current = *it;

            constexpr uint16 MaxSpins = 16;
            uint16 numSpins = 0;

            // set write mask, loop until we have exclusive access.
            uint64 localState = current->mask.BitOr(ExclusiveAccessFlag, MemoryOrder::ACQUIRE) | ExclusiveAccessFlag;

            bool hasExclusiveAccess = true;
            bool hasSharedAccess = false;

            while ((localState & SharedAccessMask))
            {
                HYP_WAIT_IDLE();
                localState = current->mask.Get(MemoryOrder::ACQUIRE);

                if (!(localState & SharedAccessMask))
                {
                    // acquired exclusively
                    break;
                }

                if (++numSpins == MaxSpins)
                {
                    // release exclusive flag, attempt to acquire shared
                    current->mask.BitAnd(~ExclusiveAccessFlag, MemoryOrder::RELEASE);
                    localState &= ~ExclusiveAccessFlag;
                    localState = current->mask.Increment(2, MemoryOrder::ACQUIRE_RELEASE);

                    hasExclusiveAccess = false;

                    if (!(localState & ExclusiveAccessFlag))
                    {
                        localState += 2;
                        hasSharedAccess = true;
                    }
                    else
                    {
                        // shared access acquire failed
                        current->mask.Decrement(2, MemoryOrder::RELEASE);
                        hasSharedAccess = false;
                    }

                    break;
                }
            }

            // Exclsuive access implies shared access
            hasSharedAccess |= hasExclusiveAccess;

            if (hasExclusiveAccess)
            {
                if (current->IsMarkedForRemoval())
                {
                    delete current;

                    it = list.Erase(it);

                    m_numProcs.Decrement(1, MemoryOrder::RELEASE);

                    continue;
                }

                // While we still have exclusive access, mark the mask for reading, so we can prevent writes while calling
                current->mask.Increment(2, MemoryOrder::RELEASE);

                // Release exclusive access
                current->mask.BitAnd(~ExclusiveAccessFlag, MemoryOrder::RELEASE);
            }
            else if (hasSharedAccess)
            {
                if (current->IsMarkedForRemoval())
                {
                    // release read access if marked for removal
                    current->mask.Decrement(2, MemoryOrder::RELEASE);
                        
                    // skip broadcast
                    continue;
                }
            }
            else
            {
                // no access - skip broadcast (read access already released due to failing to acquire read access)
                continue;
            }

            if constexpr (!std::is_void_v<ReturnType>)
            {
                HYP_CORE_ASSERT(!current->callingThreadId.IsValid() || current->callingThreadId == currentThreadId, "Cannot call a handler on a different thread if the delegate returns a value");

                if (resultConstructed)
                {
                    resultStorage.Destruct();
                }

                resultStorage.Construct(current->proc(args...));

                // Check if object has been marked for removal by our call, and if so, release the proc's memory immediately.
                // The entry will be deleted and erased on next call to Broadcast() or RemoveAllDetached()
                if (current->IsMarkedForRemoval())
                {
                    current->proc.Reset();
                }

                // Release read access
                current->mask.Decrement(2, MemoryOrder::RELEASE);

                resultConstructed = true;

                // special case for IterationResult to allow us to break from the delegate broadcast loop.
                if constexpr (std::is_same_v<ReturnType, IterationResult> || std::is_convertible_v<ReturnType, IterationResult>)
                {
                    if (IterationResult(resultStorage.Get()) == IterationResult::STOP)
                    {
                        // stop iterating
                        break;
                    }
                }

                ++it;
            }
            else
            {
                if (current->callingThreadId.IsValid() && current->callingThreadId != currentThreadId)
                {
                    current->GetCallingThread()->GetScheduler().Enqueue([current, argsTuple = Tuple<Args...>(args...)]()
                        {
                            Apply(current->proc, argsTuple);

                            if (current->IsMarkedForRemoval())
                            {
                                // free up memory for the Proc immediately, as we are not going to call it again
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

        // Append any remaining elements that were added while we were broadcasting
        ProcList& otherList = LockList(1);

        list.Concat(otherList);
        otherList.Clear();

        UnlockList(otherList);

        // swap back to the original buffer
        SwapActiveLists();

        UnlockList(list);

        if constexpr (!std::is_void_v<ReturnType>)
        {
            if (!resultConstructed)
            {
                // If no handlers were called (due to elements being removed), return a default constructed object
                return ReturnType();
            }

            ReturnType result = std::move(resultStorage).Get();
            resultStorage.Destruct();

            return result;
        }
    }

protected:
    bool Remove(DelegateHandlerEntryBase* entry)
    {
        if (!entry)
        {
            return false;
        }

        uint64 state;
        while (((state = entry->mask.Increment(2, MemoryOrder::ACQUIRE)) & ExclusiveAccessFlag))
        {
            entry->mask.Decrement(2, MemoryOrder::RELAXED);
            // wait for write flag to be released
            HYP_WAIT_IDLE();
        }

        entry->MarkForRemoval();

        entry->mask.Decrement(2, MemoryOrder::RELEASE);

        return true;
    }

    static void RemoveDelegateHandlerCallback(void* delegate, DelegateHandlerEntryBase* entry)
    {
        DelegateImpl* delegateCasted = static_cast<DelegateImpl*>(delegate);

        delegateCasted->Remove(entry);
    }

    static void DetachDelegateHandlerCallback(void* delegate, DelegateHandler&& handler)
    {
        DelegateImpl* delegateCasted = static_cast<DelegateImpl*>(delegate);

        delegateCasted->DetachDelegateHandler(std::move(handler));
    }

    /*! \brief Add a delegate handler to hang around after its DelegateHandler is destructed */
    void DetachDelegateHandler(DelegateHandler&& handler)
    {
        Mutex::Guard guard(m_detachedHandlersMutex);
        m_detachedHandlers.PushBack(std::move(handler));
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

    HYP_FORCE_INLINE void SwapActiveLists()
    {
        AtomicIncrement(&m_activeIdx);
    }

    ProcList& LockActiveList()
    {
        const int32 i = AtomicAdd(&m_activeIdx, 0) % 2;
        m_listMtx[i].Lock();

        return m_lists[i];
    }

    ProcList& LockList(int32 idx)
    {
        m_listMtx[idx].Lock();

        return m_lists[idx];
    }

    void UnlockList(ProcList& list)
    {
        const int32 i = &list == &m_lists[0] ? 0 : 1;
        m_listMtx[i].Unlock();
    }

    // double buffered list of procs to allow adding new procs while broadcasting
    ProcList m_lists[2];
    Mutex m_listMtx[2];

    // current index to write to. set to 1 while broadcasting to prevent adding new procs to the list being broadcast to.
    volatile int32 m_activeIdx;

    Array<DelegateHandler> m_detachedHandlers;
    Mutex m_detachedHandlersMutex;

    AtomicVar<uint32> m_numProcs;
    uint32 m_idCounter;
};

template <class ReturnType, class... Args>
class Delegate : public virtual IDelegate
{
public:
    friend class DelegateHandlerSet;

    Delegate()
        : m_impl(nullptr)
    {
    }

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;

    Delegate(Delegate&& other) noexcept
        : m_impl(other.m_impl)
    {
        other.m_impl = nullptr;
    }

    Delegate& operator=(Delegate&& other) noexcept = delete;

    virtual ~Delegate() override
    {
        // Ensure that the delegate is not being used by any threads before deleting it
        TUniqueLock guard(m_mtx);

        if (m_impl != nullptr)
        {
            delete m_impl;
            m_impl = nullptr;
        }
        // no need to unlock, as the spinlock is being destroyed
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
        TSharedLock guard(m_mtx);

        if (!m_impl)
        {
            return false;
        }

        return m_impl->AnyBound();
    }

    /*! \brief Bind a Proc<> to the Delegate.
     *  \note The handler will be removed when the last reference to the returned DelegateHandler is removed.
     *  This makes it easy to manage resource cleanup, as you can store the DelegateHandler as a class member and when the object is destroyed, the handler will be removed from the Delegate.
     *
     *  \param proc The Proc to bind.
     *  \return  A reference counted DelegateHandler object that can be used to remove the handler from the Delegate. */
    HYP_NODISCARD DelegateHandler Bind(Proc<ReturnType(Args...)>&& proc)
    {
        TSharedLock guard(m_mtx);

        if (!m_impl)
        {
            guard.Reset();

            {
                TUniqueLock guard2(m_mtx);

                // check still nullptr after acquiring unique lock
                if (!m_impl)
                    m_impl = new DelegateImpl<ReturnType, Args...>();
            }

            guard.Reset(m_mtx);
        }

        return m_impl->Bind(std::move(proc));
    }

    /*! \brief Bind a Proc<> to the Delegate.
     *  \note The handler will be removed when the last reference to the returned DelegateHandler is removed.
     *  This makes it easy to manage resource cleanup, as you can store the DelegateHandler as a class member and when the object is destroyed, the handler will be removed from the Delegate.
     *
     *  \param proc The Proc to bind.
     *  \param callingThreadId The thread to call the bound function on.
     *  \return  A reference counted DelegateHandler object that can be used to remove the handler from the Delegate. */
    HYP_NODISCARD DelegateHandler BindThreaded(Proc<ReturnType(Args...)>&& proc, const ThreadId& callingThreadId)
    {
        TSharedLock guard(m_mtx);

        if (!m_impl)
        {
            guard.Reset();

            {
                TUniqueLock guard2(m_mtx);
                
                // check still nullptr after acquiring unique lock
                if (!m_impl)
                    m_impl = new DelegateImpl<ReturnType, Args...>();
            }

            guard.Reset(m_mtx);
        }

        return m_impl->BindThreaded(std::move(proc), callingThreadId);
    }

    /*! \brief Remove all detached handlers from the Delegate.
     *  \note Only detached handlers are removed, as removing bound handlers would cause them to hold dangling pointers.
     *  \return The number of handlers removed. */
    int RemoveAllDetached() override
    {
        TSharedLock guard(m_mtx);

        if (!m_impl)
        {
            return 0;
        }

        return m_impl->RemoveAllDetached();
    }

    bool Remove(DelegateHandler&& handle) override
    {
        TSharedLock guard(m_mtx);

        if (!m_impl)
        {
            return false;
        }

        return m_impl->Remove(std::move(handle));
    }

    /*! \brief Broadcast a message to all bound handlers.
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref ReturnType if no handlers were bound. */
    template <class... ArgTypes>
    ReturnType Broadcast(ArgTypes&&... args)
    {
        TSharedLock guard(m_mtx);

        if (!m_impl)
        {
            if constexpr (!std::is_void_v<ReturnType>)
            {
                return ReturnType();
            }
            else
            {
                return;
            }
        }

        return m_impl->Broadcast(std::forward<ArgTypes>(args)...);
    }

    /*! \brief Call operator overload - alias method for Broadcast().
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref ReturnType if no handlers were bound. */
    template <class... ArgTypes>
    HYP_FORCE_INLINE ReturnType operator()(ArgTypes&&... args) const
    {
        return const_cast<Delegate*>(this)->Broadcast(std::forward<ArgTypes>(args)...);
    }

private:
    using DelegateImplType = DelegateImpl<ReturnType, Args...>;

    // keep implementation pointer to reduce static memory footprint as many delegates will not have any handlers bound
    DelegateImplType* m_impl;
    // spinlock to protect against multiple threads creating / reading from m_impl pointer
    SharedMutex m_mtx;
};

/*! \brief Stores a set of DelegateHandlers, intended to hold references to delegates and remove them upon destruction of the owner object. */
class DelegateHandlerSet : HashMap<Name, DelegateHandler, DynamicNodeAllocator>
{
public:
    using HashMap::ConstIterator;
    using HashMap::Iterator;

    HYP_FORCE_INLINE DelegateHandlerSet& Add(DelegateHandler&& delegateHandler)
    {
        HashMap::Insert({ Name::Unique("DelegateHandler_"), std::move(delegateHandler) });
        return *this;
    }

    HYP_FORCE_INLINE DelegateHandlerSet& Add(Name name, DelegateHandler&& delegateHandler)
    {
        HashMap::Insert({ name, std::move(delegateHandler) });
        return *this;
    }

    HYP_FORCE_INLINE bool Remove(StringHash name)
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

    /*! \brief Remove all delegate handlers that are bound to the given \p delegate
     *  \returns The number of delegate handlers that were removed. */
    template <class ReturnType, class... Args>
    HYP_FORCE_INLINE int Remove(Delegate<ReturnType, Args...>* delegate)
    {
        if (!delegate)
        {
            HYP_CORE_ASSERT(false, "Cannot remove delegate handlers from a null delegate");
            return 0;
        }

        // lock the delegate object for reading
        TSharedLock guard(delegate->m_mtx);

        Array<DelegateHandler> delegateHandlers;

        for (auto it = HashMap::Begin(); it != HashMap::End();)
        {
            if (it->second.delegateImpl == delegate->m_impl)
            {
                delegateHandlers.PushBack(std::move(it->second));

                it = HashMap::Erase(it);

                continue;
            }

            ++it;
        }

        return int(delegateHandlers.Size());
    }

    HYP_FORCE_INLINE Iterator Find(StringHash name)
    {
        return HashMap::FindAs(name);
    }

    HYP_FORCE_INLINE ConstIterator Find(StringHash name) const
    {
        return HashMap::FindAs(name);
    }

    HYP_FORCE_INLINE bool Contains(StringHash name) const
    {
        return HashMap::FindAs(name) != HashMap::End();
    }

    HYP_DEF_STL_BEGIN_END(HashMap::Begin(), HashMap::End())
};

template <class T>
struct IsDelegate : std::false_type
{
};

template <class ReturnType, class... Args>
struct IsDelegate<Delegate<ReturnType, Args...>> : std::true_type
{
};

template <class T>
inline constexpr bool IsDelegateV = IsDelegate<T>::value;

} // namespace functional

using functional::Delegate;
using functional::DelegateHandler;
using functional::DelegateHandlerSet;
using functional::IDelegate;
using functional::IsDelegate;
using functional::IsDelegateV;

} // namespace Hyperion
