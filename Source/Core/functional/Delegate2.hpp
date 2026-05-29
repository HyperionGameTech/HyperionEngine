/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/functional/Proc.hpp>

#include <Core/containers/Array.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/threading/SharedMutex.hpp>
#include <Core/threading/Threads.hpp>
#include <Core/threading/Task.hpp>
#include <Core/threading/Scheduler.hpp>

#include <Core/utilities/ValueStorage.hpp>
#include <Core/utilities/Tuple.hpp>
#include <Core/utilities/ForEach.hpp>

#include <Core/name/Name.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

namespace threading {

class ThreadId;
class ThreadBase;

CORE_API extern void ThreadSleep(uint32 milliseconds);
CORE_API extern ThreadBase *GetThreadById(const ThreadId &threadId);

} // namespace threading

using threading::GetThreadById;
using threading::ThreadBase;
using threading::ThreadSleep;

namespace functional {

template <class ReturnType, class... Args>
class Delegate2;

struct Delegate2Handler;

struct Delegate2HandlerEntryBase
{
    uint32 index;
    ThreadId callingThreadId;

    AtomicVar<bool> markedForRemoval { false };

    /*! \brief Number of active broadcasts currently executing (or about to execute) this handler.
     *  When > 0, the entry's Proc must not be reset or deleted. */
    AtomicVar<int32> readCount { 0 };

    /*! \brief Set to true once the Proc has been reset.
     *  Protected by CompareExchange so only one thread performs the Reset. */
    AtomicVar<bool> procWasReset { false };

    ~Delegate2HandlerEntryBase()
    {
        while (HYP_UNLIKELY(readCount.Get(MemoryOrder::ACQUIRE) > 0))
        {
            HYP_NAMED_SCOPE("~Delegate2HandlerEntryBase() - waiting for readers to drain");
            ThreadSleep(0);
        }
    }

    HYP_FORCE_INLINE void MarkForRemoval()
    {
        markedForRemoval.Set(true, MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE bool IsMarkedForRemoval() const
    {
        return markedForRemoval.Get(MemoryOrder::ACQUIRE);
    }

    ThreadBase *GetCallingThread() const
    {
        if (!callingThreadId.IsValid())
        {
            return nullptr;
        }

        ThreadBase *thread = GetThreadById(callingThreadId);
        HYP_CORE_ASSERT(thread != nullptr);

        return thread;
    }
};

template <class ProcType>
struct Delegate2HandlerEntry final : Delegate2HandlerEntryBase
{
    ProcType proc;

    Delegate2HandlerEntry()
    {
        index = ~0u;
    }
};

struct Delegate2Handler
{
    Delegate2HandlerEntryBase *entry;
    void *delegateImpl;
    void (*removeFn)(void *, Delegate2HandlerEntryBase *);
    void (*detachFn)(void *, Delegate2Handler &&delegateHandler);

    Delegate2Handler()
        : entry(nullptr),
          delegateImpl(nullptr),
          removeFn(nullptr),
          detachFn(nullptr)
    {
    }

    Delegate2Handler(Delegate2HandlerEntryBase *entry, void *delegateImpl,
                     void (*removeFn)(void *, Delegate2HandlerEntryBase *),
                     void (*detachFn)(void *, Delegate2Handler &&delegateHandler))
        : entry(entry),
          delegateImpl(delegateImpl),
          removeFn(removeFn),
          detachFn(detachFn)
    {
    }

    Delegate2Handler(const Delegate2Handler &other) = delete;
    Delegate2Handler &operator=(const Delegate2Handler &other) = delete;

    Delegate2Handler(Delegate2Handler &&other) noexcept
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

    Delegate2Handler &operator=(Delegate2Handler &&other) noexcept
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

    ~Delegate2Handler()
    {
        Reset();
    }

    HYP_FORCE_INLINE void *GetDelegate() const
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

    HYP_FORCE_INLINE bool operator==(const Delegate2Handler &other) const
    {
        return entry == other.entry && delegateImpl == other.delegateImpl;
    }

    HYP_FORCE_INLINE bool operator!=(const Delegate2Handler &other) const
    {
        return entry != other.entry || delegateImpl != other.delegateImpl;
    }

    HYP_FORCE_INLINE bool operator<(const Delegate2Handler &other) const
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

template <class ReturnType, class... Args>
class Delegate2Impl final
{
    using ProcType = Proc<ReturnType(Args...)>;
    using EntryType = Delegate2HandlerEntry<ProcType>;
    using ProcList = Array<EntryType *>;

public:
    Delegate2Impl()
        : m_numProcs(0),
          m_idCounter(0),
          m_activeIdx(0)
    {
    }

    Delegate2Impl(const Delegate2Impl &other) = delete;
    Delegate2Impl &operator=(const Delegate2Impl &other) = delete;

    Delegate2Impl(Delegate2Impl &&other) noexcept = delete;
    Delegate2Impl &operator=(Delegate2Impl &&other) noexcept = delete;

    ~Delegate2Impl()
    {
        m_detachedHandlers.Clear();

        AssertDebug(m_activeIdx % 2 == 0);
        AssertDebug(m_lists[1].Empty());

        for (EntryType *entry : m_lists[0])
        {
            while (HYP_UNLIKELY(entry->readCount.Get(MemoryOrder::ACQUIRE) > 0))
            {
                HYP_NAMED_SCOPE("~Delegate2Impl() - waiting for readers to drain");
                ThreadSleep(0);
            }

            delete entry;
        }
    }

    HYP_FORCE_INLINE bool AnyBound() const
    {
        return m_numProcs.Get(MemoryOrder::ACQUIRE) != 0;
    }

    HYP_NODISCARD Delegate2Handler Bind(ProcType &&proc)
    {
        ProcList &list = LockActiveList();

        EntryType *entry = list.PushBack(new EntryType());
        entry->index = m_idCounter++;
        entry->callingThreadId = ThreadId::Invalid();
        entry->proc = std::move(proc);

        m_numProcs.Increment(1, MemoryOrder::RELEASE);

        Delegate2Handler result = CreateDelegate2Handler(entry);

        UnlockList(list);

        return result;
    }

    HYP_NODISCARD Delegate2Handler BindThreaded(ProcType &&proc, const ThreadId &callingThreadId)
    {
        HYP_CORE_ASSERT(std::is_void_v<ReturnType> || !callingThreadId.IsValid() || callingThreadId == ThreadId::Current(),
                        "Cannot bind a return-value handler to a different thread");

#if HYP_DEBUG_MODE
        if (callingThreadId != ThreadId::Invalid())
        {
            HYP_CORE_ASSERT(GetThreadById(callingThreadId) != nullptr,
                            "Cannot bind a handler to a thread that is not registered");
        }
#endif

        ProcList &list = LockActiveList();

        EntryType *entry = list.PushBack(new EntryType());
        entry->index = m_idCounter++;
        entry->callingThreadId = callingThreadId;
        entry->proc = std::move(proc);

        m_numProcs.Increment(1, MemoryOrder::RELEASE);

        Delegate2Handler result = CreateDelegate2Handler(entry);

        UnlockList(list);

        return result;
    }

    int RemoveAllDetached()
    {
        if (!AnyBound())
        {
            return 0;
        }

        Mutex::Guard guard(m_detachedHandlersMutex);
        m_detachedHandlers.Clear();

        int numRemoved = 0;

        ProcList &list = LockList(0);

        for (auto it = list.Begin(); it != list.End();)
        {
            EntryType *current = *it;

            if (current->IsMarkedForRemoval())
            {
                if (current->readCount.Get(MemoryOrder::ACQUIRE) == 0)
                {
                    delete current;

                    it = list.Erase(it);

                    ++numRemoved;

                    continue;
                }
            }

            ++it;
        }

        m_numProcs.Decrement(uint32(numRemoved), MemoryOrder::RELEASE);

        UnlockList(list);

        return numRemoved;
    }

    bool Remove(Delegate2Handler &&handle)
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

    template <class... ArgTypes>
    ReturnType Broadcast(ArgTypes &&...args)
    {
        if (!AnyBound())
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

        const ThreadId currentThreadId = CurrentThreadId();

        ValueStorage<ReturnType> resultStorage;
        bool resultConstructed = false;

        ProcList &list = LockList(0);
        SwapActiveLists();

        for (auto it = list.Begin(); it != list.End();)
        {
            EntryType *current = *it;

            if (current->IsMarkedForRemoval())
            {
                if (current->readCount.Get(MemoryOrder::ACQUIRE) == 0)
                {
                    delete current;

                    it = list.Erase(it);

                    m_numProcs.Decrement(1, MemoryOrder::RELEASE);
                }
                else
                {
                    ++it;
                }

                continue;
            }

            current->readCount.Increment(1, MemoryOrder::ACQUIRE);

            if (HYP_UNLIKELY(current->IsMarkedForRemoval()))
            {
                current->readCount.Decrement(1, MemoryOrder::RELEASE);
                ++it;
                continue;
            }

            if constexpr (!std::is_void_v<ReturnType>)
            {
                HYP_CORE_ASSERT(!current->callingThreadId.IsValid() || current->callingThreadId == currentThreadId,
                                "Cannot call a return-value handler on a different thread");

                if (resultConstructed)
                {
                    resultStorage.Destruct();
                }

                resultStorage.Construct(current->proc(args...));

                resultConstructed = true;

                if (current->IsMarkedForRemoval())
                {
                    if (TryResetProc(current))
                    {
                        m_numProcs.Decrement(1, MemoryOrder::RELEASE);
                    }
                }

                current->readCount.Decrement(1, MemoryOrder::RELEASE);

                if constexpr (std::is_same_v<ReturnType, IterationResult> || std::is_convertible_v<ReturnType, IterationResult>)
                {
                    if (IterationResult(resultStorage.Get()) == IterationResult::STOP)
                    {
                        break;
                    }
                }

                ++it;
            }
            else
            {
                if (current->callingThreadId.IsValid() && current->callingThreadId != currentThreadId)
                {
                    current->GetCallingThread()->GetScheduler().Enqueue(
                        [current, argsTuple = Tuple<Args...>(args...)]()
                        {
                            if (!current->IsMarkedForRemoval())
                            {
                                Apply(current->proc, argsTuple);
                            }

                            if (current->IsMarkedForRemoval())
                            {
                                TryResetProc(current);
                            }

                            current->readCount.Decrement(1, MemoryOrder::RELEASE);
                        },
                        TaskEnqueueFlags::FIRE_AND_FORGET);
                }
                else
                {
                    current->proc(args...);

                    if (current->IsMarkedForRemoval())
                    {
                        if (TryResetProc(current))
                        {
                            m_numProcs.Decrement(1, MemoryOrder::RELEASE);
                        }
                    }

                    current->readCount.Decrement(1, MemoryOrder::RELEASE);
                }

                ++it;
            }
        }

        ProcList &otherList = LockList(1);

        list.Concat(otherList);
        otherList.Clear();

        UnlockList(otherList);

        SwapActiveLists();

        UnlockList(list);

        if constexpr (!std::is_void_v<ReturnType>)
        {
            if (!resultConstructed)
            {
                return ReturnType();
            }

            ReturnType result = std::move(resultStorage).Get();
            resultStorage.Destruct();

            return result;
        }
    }

protected:
    /*! \brief Attempt to safely reset the proc on a marked entry.
     *  Only one caller will succeed (via CAS). Returns true if this caller performed the reset. */
    HYP_FORCE_INLINE bool TryResetProc(EntryType *entry)
    {
        bool expected = false;
        if (entry->procWasReset.CompareExchangeStrong(expected, true, MemoryOrder::SEQUENTIAL))
        {
            entry->proc.Reset();
            return true;
        }

        return false;
    }

    /*! \brief Remove a handler entry.
     *
     *  Marks the entry for removal and waits for any in-flight broadcast to finish
     *  before safely resetting the Proc to release captured resources.
     *
     *  The entry's memory is freed during the next Broadcast() or RemoveAllDetached() call.
     *
     *  Self-removal during broadcast is handled gracefully: the broadcast itself will reset
     *  the proc after the call returns, and Remove() will detect this via the procWasReset flag. */
    bool Remove(Delegate2HandlerEntryBase *baseEntry)
    {
        if (!baseEntry)
        {
            return false;
        }

        EntryType *entry = static_cast<EntryType *>(baseEntry);

        entry->readCount.Increment(1, MemoryOrder::ACQUIRE);

        entry->MarkForRemoval();

        while (HYP_UNLIKELY(entry->readCount.Get(MemoryOrder::ACQUIRE) > 1))
        {
            HYP_NAMED_SCOPE("Delegate2Impl::Remove() - waiting for readers to drain");
            ThreadSleep(0);
        }

        if (TryResetProc(entry))
        {
            m_numProcs.Decrement(1, MemoryOrder::RELEASE);
        }

        entry->readCount.Decrement(1, MemoryOrder::RELEASE);

        return true;
    }

    static void RemoveDelegate2HandlerCallback(void *delegate, Delegate2HandlerEntryBase *entry)
    {
        Delegate2Impl *delegateCasted = static_cast<Delegate2Impl *>(delegate);

        delegateCasted->Remove(entry);
    }

    static void DetachDelegate2HandlerCallback(void *delegate, Delegate2Handler &&handler)
    {
        Delegate2Impl *delegateCasted = static_cast<Delegate2Impl *>(delegate);

        delegateCasted->DetachDelegate2Handler(std::move(handler));
    }

    void DetachDelegate2Handler(Delegate2Handler &&handler)
    {
        Mutex::Guard guard(m_detachedHandlersMutex);
        m_detachedHandlers.PushBack(std::move(handler));
    }

    HYP_FORCE_INLINE Delegate2Handler CreateDelegate2Handler(EntryType *entry)
    {
        return Delegate2Handler {
            entry,
            static_cast<void *>(this),
            RemoveDelegate2HandlerCallback,
            DetachDelegate2HandlerCallback
        };
    }

    HYP_FORCE_INLINE void SwapActiveLists()
    {
        AtomicIncrement(&m_activeIdx);
    }

    HYP_FORCE_INLINE ProcList &LockActiveList()
    {
        const int32 i = AtomicAdd(&m_activeIdx, 0) % 2;
        m_listMtx[i].Lock();

        return m_lists[i];
    }

    HYP_FORCE_INLINE ProcList &LockList(int32 idx)
    {
        m_listMtx[idx].Lock();

        return m_lists[idx];
    }

    HYP_FORCE_INLINE void UnlockList(ProcList &list)
    {
        const int32 i = &list == &m_lists[0] ? 0 : 1;
        m_listMtx[i].Unlock();
    }

    ProcList m_lists[2];
    Mutex m_listMtx[2];

    volatile int32 m_activeIdx;

    Array<Delegate2Handler> m_detachedHandlers;
    Mutex m_detachedHandlersMutex;

    AtomicVar<uint32> m_numProcs;
    uint32 m_idCounter;
};

template <class ReturnType, class... Args>
class Delegate2
{
public:
    friend class Delegate2HandlerSet;

    Delegate2()
        : m_impl(nullptr)
    {
    }

    Delegate2(const Delegate2 &other) = delete;
    Delegate2 &operator=(const Delegate2 &other) = delete;

    Delegate2(Delegate2 &&other) noexcept
        : m_impl(other.m_impl)
    {
        other.m_impl = nullptr;
    }

    Delegate2 &operator=(Delegate2 &&other) noexcept = delete;

    ~Delegate2()
    {
        TUniqueLock guard(m_mutex);

        if (m_impl != nullptr)
        {
            delete m_impl;
            m_impl = nullptr;
        }
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !AnyBound();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return AnyBound();
    }

    bool AnyBound() const
    {
        TSharedLock guard(m_mutex);

        if (!m_impl)
        {
            return false;
        }

        return m_impl->AnyBound();
    }

    HYP_NODISCARD Delegate2Handler Bind(Proc<ReturnType(Args...)> &&proc)
    {
        TSharedLock guard(m_mutex);

        if (!m_impl)
        {
            guard.Reset();

            {
                TUniqueLock guard2(m_mutex);

                if (!m_impl)
                {
                    m_impl = new Delegate2Impl<ReturnType, Args...>();
                }
            }

            guard.Reset(m_mutex);
        }

        return m_impl->Bind(std::move(proc));
    }

    HYP_NODISCARD Delegate2Handler BindThreaded(Proc<ReturnType(Args...)> &&proc, const ThreadId &callingThreadId)
    {
        TSharedLock guard(m_mutex);

        if (!m_impl)
        {
            guard.Reset();

            {
                TUniqueLock guard2(m_mutex);

                if (!m_impl)
                {
                    m_impl = new Delegate2Impl<ReturnType, Args...>();
                }
            }

            guard.Reset(m_mutex);
        }

        return m_impl->BindThreaded(std::move(proc), callingThreadId);
    }

    int RemoveAllDetached()
    {
        TSharedLock guard(m_mutex);

        if (!m_impl)
        {
            return 0;
        }

        return m_impl->RemoveAllDetached();
    }

    bool Remove(Delegate2Handler &&handle)
    {
        TSharedLock guard(m_mutex);

        if (!m_impl)
        {
            return false;
        }

        return m_impl->Remove(std::move(handle));
    }

    template <class... ArgTypes>
    ReturnType Broadcast(ArgTypes &&...args)
    {
        TSharedLock guard(m_mutex);

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

    template <class... ArgTypes>
    HYP_FORCE_INLINE ReturnType operator()(ArgTypes &&...args) const
    {
        return const_cast<Delegate2 *>(this)->Broadcast(std::forward<ArgTypes>(args)...);
    }

private:
    using Delegate2ImplType = Delegate2Impl<ReturnType, Args...>;

    Delegate2ImplType *m_impl;
    SharedMutex m_mutex;
};

class Delegate2HandlerSet : TMap<Name, Delegate2Handler, DynamicAllocator, HashTablePolicy::NotPooled>
{
public:
    using TMap::ConstIterator;
    using TMap::Iterator;

    HYP_FORCE_INLINE Delegate2HandlerSet &Add(Delegate2Handler &&delegateHandler)
    {
        TMap::Insert({ Name::Unique("Delegate2Handler_"), std::move(delegateHandler) });
        return *this;
    }

    HYP_FORCE_INLINE Delegate2HandlerSet &Add(Name name, Delegate2Handler &&delegateHandler)
    {
        TMap::Insert({ name, std::move(delegateHandler) });
        return *this;
    }

    HYP_FORCE_INLINE bool Remove(StringHash name)
    {
        auto it = TMap::FindAs(name);

        if (it == TMap::End())
        {
            return false;
        }

        TMap::Erase(it);

        return true;
    }

    HYP_FORCE_INLINE bool Remove(ConstIterator it)
    {
        if (it == TMap::End())
        {
            return false;
        }

        TMap::Erase(it);

        return true;
    }

    template <class ReturnType, class... Args>
    HYP_FORCE_INLINE int Remove(Delegate2<ReturnType, Args...> *delegate)
    {
        if (!delegate)
        {
            HYP_CORE_ASSERT(false, "Cannot remove delegate handlers from a null delegate");
            return 0;
        }

        TSharedLock guard(delegate->m_mutex);

        Array<Delegate2Handler> delegateHandlers;

        for (auto it = TMap::Begin(); it != TMap::End();)
        {
            if (it->second.delegateImpl == delegate->m_impl)
            {
                delegateHandlers.PushBack(std::move(it->second));

                it = TMap::Erase(it);

                continue;
            }

            ++it;
        }

        return int(delegateHandlers.Size());
    }

    HYP_FORCE_INLINE Iterator Find(StringHash name)
    {
        return TMap::FindAs(name);
    }

    HYP_FORCE_INLINE ConstIterator Find(StringHash name) const
    {
        return TMap::FindAs(name);
    }

    HYP_FORCE_INLINE bool Contains(StringHash name) const
    {
        return TMap::FindAs(name) != TMap::End();
    }

    HYP_DEF_STL_BEGIN_END(TMap::Begin(), TMap::End())
};

template <class T>
struct IsDelegate2 : std::false_type
{
};

template <class ReturnType, class... Args>
struct IsDelegate2<Delegate2<ReturnType, Args...>> : std::true_type
{
};

template <class T>
inline constexpr bool IsDelegate2V = IsDelegate2<T>::value;

} // namespace functional

using functional::Delegate2;
using functional::Delegate2Handler;
using functional::Delegate2HandlerSet;
using functional::IsDelegate2;
using functional::IsDelegate2V;

} // namespace Hyperion
