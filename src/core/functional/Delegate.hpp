/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DELEGATE_HPP
#define HYPERION_DELEGATE_HPP

#include <core/functional/Proc.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/IDGenerator.hpp>

#include <core/Name.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

// Leave enabled unless you want to crash the engine
#define HYP_DELEGATE_THREAD_SAFE

namespace hyperion {

namespace dotnet {
class Object;
} // namespace dotnet

namespace functional {

class IDelegate;

template <class ReturnType, class... Args>
class Delegate;

class DelegateHandler;

namespace detail {

struct DelegateHandlerData
{
    uint32  id;
    void    *delegate;
    void    (*remove_fn)(void *, uint32);
    void    (*detach_fn)(void *, DelegateHandler &&delegate_handler);

    DelegateHandlerData(uint32 id, void *delegate, void(*remove_fn)(void *, uint32), void(*detach_fn)(void *, DelegateHandler &&delegate_handler))
        : id(id),
          delegate(delegate),
          remove_fn(remove_fn),
          detach_fn(detach_fn)
    {
    }

    DelegateHandlerData(const DelegateHandlerData &other)               = delete;
    DelegateHandlerData &operator=(const DelegateHandlerData &other)    = delete;

    DelegateHandlerData(DelegateHandlerData &&other) noexcept
        : id(other.id),
          delegate(other.delegate),
          remove_fn(other.remove_fn),
          detach_fn(other.detach_fn)
    {
        other.id = 0;
        other.delegate = nullptr;
        other.remove_fn = nullptr;
        other.detach_fn = nullptr;
    }

    DelegateHandlerData &operator=(DelegateHandlerData &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (IsValid()) {
            AssertThrow(remove_fn != nullptr);

            remove_fn(delegate, id);
        }

        id = other.id;
        delegate = other.delegate;
        remove_fn = other.remove_fn;
        detach_fn = other.detach_fn;

        other.id = 0;
        other.delegate = nullptr;
        other.remove_fn = nullptr;
        other.detach_fn = nullptr;

        return *this;
    }

    HYP_API ~DelegateHandlerData();

    HYP_FORCE_INLINE uint32 GetID() const
        { return id; }

    HYP_FORCE_INLINE void *GetDelegate() const
        { return delegate; }

    HYP_API void Reset();
    HYP_API void Detach(DelegateHandler &&delegate_handler);

    HYP_FORCE_INLINE bool IsValid() const
        { return id != 0 && delegate != nullptr; }

    HYP_FORCE_INLINE bool operator==(const DelegateHandlerData &other) const
        { return id == other.id && delegate == other.delegate; }

    HYP_FORCE_INLINE bool operator!=(const DelegateHandlerData &other) const
        { return id != other.id || delegate != other.delegate; }

    HYP_FORCE_INLINE bool operator<(const DelegateHandlerData &other) const
        { return id < other.id; }
};

template <class ProcType>
struct DelegateHandlerProc
{
    RC<ProcType>    proc;
    ThreadID        calling_thread_id;

    IThread *GetCallingThread() const
    {
        if (!calling_thread_id.IsValid()) {
            return nullptr;
        }

        IThread *thread = Threads::GetThread(calling_thread_id);
        AssertThrow(thread != nullptr);

        return thread;
    }
};

} // namespace detail

/*! \brief Holds a reference to a DelegateHandlerData object.
    When all references to the underlying object are gone, the handler will be removed from the Delegate.
 */
class DelegateHandler
{
public:
    template <class ReturnType, class... Args>
    friend class Delegate;

    friend class DelegateHandlerSet;

    DelegateHandler()                                               = default;

    DelegateHandler(const DelegateHandler &other)                   = default;
    DelegateHandler &operator=(const DelegateHandler &other)        = default;

    DelegateHandler(DelegateHandler &&other) noexcept
        : m_data(std::move(other.m_data))
    {
    }

    DelegateHandler &operator=(DelegateHandler &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        m_data = std::move(other.m_data);

        return *this;
    }

    ~DelegateHandler()                                              = default;

    HYP_FORCE_INLINE bool operator==(const DelegateHandler &other) const
        { return m_data == other.m_data; }

    HYP_FORCE_INLINE bool operator!=(const DelegateHandler &other) const
        { return m_data != other.m_data; }

    HYP_FORCE_INLINE bool operator<(const DelegateHandler &other) const
    {
        if (!IsValid()) {
            return false;
        }

        if (!other.IsValid()) {
            return true;
        }

        return *m_data < *other.m_data;
    }

    /*! \brief Check if the DelegateHandler is valid.
     *
     * \return True if the DelegateHandler is valid, false otherwise.
     */
    HYP_FORCE_INLINE bool IsValid() const
        { return m_data != nullptr && m_data->IsValid(); }

    /*! \brief Reset the DelegateHandler to an invalid state. */
    HYP_FORCE_INLINE void Reset()
    {
        m_data.Reset();
    }

    /*! \brief Detach the DelegateHandler from the Delegate.
        This will allow the Delegate handler function to remain attached to the delegate upon destruction of this object.
        \note This requires proper management to prevent memory leaks and access of invalid objects, as the lifecycle of the handler will now last as long as the Delegate itself. */
    HYP_FORCE_INLINE void Detach()
    {
        if (IsValid()) {
            m_data->Detach(std::move(*this));
        }
    }

private:
    DelegateHandler(RC<functional::detail::DelegateHandlerData> data)
        : m_data(std::move(data))
    {
    }

    RC<functional::detail::DelegateHandlerData> m_data;
};

/*! \brief Stores a set of DelegateHandlers, intended to hold references to delegates and remove them upon destruction of the owner object. */
class DelegateHandlerSet
{
public:
    using Iterator = typename FlatMap<Name, DelegateHandler>::Iterator;
    using ConstIterator = typename FlatMap<Name, DelegateHandler>::ConstIterator;

    HYP_FORCE_INLINE DelegateHandlerSet &Add(const DelegateHandler &delegate_handler)
    {
        m_delegate_handlers.Insert({ Name::Unique("DelegateHandler_"), delegate_handler });
        return *this;
    }

    HYP_FORCE_INLINE DelegateHandlerSet &Add(DelegateHandler &&delegate_handler)
    {
        m_delegate_handlers.Insert({ Name::Unique("DelegateHandler_"), std::move(delegate_handler) });
        return *this;
    }

    HYP_FORCE_INLINE DelegateHandlerSet &Add(Name name, const DelegateHandler &delegate_handler)
    {
        m_delegate_handlers.Insert({ name, delegate_handler });
        return *this;
    }

    HYP_FORCE_INLINE DelegateHandlerSet &Add(Name name, DelegateHandler &&delegate_handler)
    {
        m_delegate_handlers.Insert({ name, std::move(delegate_handler) });
        return *this;
    }

    HYP_FORCE_INLINE bool Remove(WeakName name)
    {
        auto it = m_delegate_handlers.FindAs(name);

        if (it == m_delegate_handlers.End()) {
            return false;
        }

        m_delegate_handlers.Erase(it);

        return true;
    }

    HYP_FORCE_INLINE bool Remove(ConstIterator it)
    {
        if (it == m_delegate_handlers.End()) {
            return false;
        }

        m_delegate_handlers.Erase(it);

        return true;
    }

    /*! \brief Remove all delegate handlers that are bound to the given \ref{delegate}
     *  \returns The number of delegate handlers that were removed. */
    template <class ReturnType, class... Args>
    HYP_FORCE_INLINE int Remove(Delegate<ReturnType, Args...> *delegate)
    {
        Array<DelegateHandler> delegate_handlers;

        for (auto it =  m_delegate_handlers.Begin(); it != m_delegate_handlers.End();) {
            if (it->second.m_data != nullptr && it->second.m_data->delegate == delegate) {
                delegate_handlers.PushBack(std::move(it->second));

                it = m_delegate_handlers.Erase(it);

                continue;
            }

            ++it;
        }

        return int(delegate_handlers.Size());
    }

    HYP_FORCE_INLINE Iterator Find(WeakName name)
        { return m_delegate_handlers.FindAs(name); }

    HYP_FORCE_INLINE ConstIterator Find(WeakName name) const
        { return m_delegate_handlers.FindAs(name); }

    HYP_FORCE_INLINE bool Contains(WeakName name) const
        { return m_delegate_handlers.Contains(name); }

    HYP_DEF_STL_BEGIN_END(
        m_delegate_handlers.Begin(),
        m_delegate_handlers.End()
    )

private:
    FlatMap<Name, DelegateHandler>  m_delegate_handlers;
};

class IDelegate
{
public:
    virtual ~IDelegate() = default;

    virtual bool AnyBound() const = 0;

    virtual bool Remove(uint32 id) = 0;
    virtual bool Remove(const DelegateHandler &handler) = 0;
    virtual int RemoveAll(bool thread_safe = true) = 0;
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
    using ProcType = Proc<ReturnType, Args...>;

    Delegate()
        : m_id_counter(0)
#ifdef HYP_DELEGATE_THREAD_SAFE
        , m_num_procs(0)
#endif
    {
    }

    Delegate(const Delegate &other)                 = delete;
    Delegate &operator=(const Delegate &other)      = delete;

    Delegate(Delegate &&other) noexcept
        : m_procs(std::move(other.m_procs)),
#ifdef HYP_DELEGATE_THREAD_SAFE
          m_num_procs(other.m_num_procs.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
#endif
          m_detached_handlers(std::move(other.m_detached_handlers)),
          m_id_counter(other.m_id_counter)
    {
        other.m_id_counter = 0;
    }

    Delegate &operator=(Delegate &&other) noexcept  = delete;

    virtual ~Delegate() override
    {
        m_detached_handlers.Clear();
    }

    HYP_FORCE_INLINE bool operator!() const
        { return !AnyBound(); }

    HYP_FORCE_INLINE explicit operator bool() const
        { return AnyBound(); }

    virtual bool AnyBound() const override
    {
#ifdef HYP_DELEGATE_THREAD_SAFE
        return m_num_procs.Get(MemoryOrder::ACQUIRE) != 0;
#else
        return m_procs.Any();
#endif
    }

    /*! \brief Bind a Proc<> to the Delegate. The bound function will always be called on the thread that Bind() is called from if \ref{require_current_thread} is set to true.
     *  \note The handler will be removed when the last reference to the returned DelegateHandler is removed.
     *  This makes it easy to manage resource cleanup, as you can store the DelegateHandler as a class member and when the object is destroyed, the handler will be removed from the Delegate.
     *
     *  \param proc The Proc to bind.
     *  \param require_current_thread Should the delegate handler function be called on the current thread?
     *  \return  A reference counted DelegateHandler object that can be used to remove the handler from the Delegate. */
    HYP_NODISCARD DelegateHandler Bind(ProcType &&proc, bool require_current_thread = false)
    {
        AssertDebugMsg(std::is_void_v<ReturnType> || !require_current_thread, "Cannot use require_current_thread for non-void delegate return type");

        return Bind(std::move(proc), require_current_thread ? ThreadID::Current() : ThreadID());
    }

    /*! \brief Bind a Proc<> to the Delegate.
     *  \note The handler will be removed when the last reference to the returned DelegateHandler is removed.
     *  This makes it easy to manage resource cleanup, as you can store the DelegateHandler as a class member and when the object is destroyed, the handler will be removed from the Delegate.
     *
     *  \param proc The Proc to bind.
     *  \param calling_thread_id The thread to call the bound function on.
     *  \return  A reference counted DelegateHandler object that can be used to remove the handler from the Delegate. */
    HYP_NODISCARD DelegateHandler Bind(ProcType &&proc, const ThreadID &calling_thread_id)
    {
        AssertDebugMsg(std::is_void_v<ReturnType> || !calling_thread_id.IsValid() || calling_thread_id == ThreadID::Current(), "Cannot call a handler on a different thread if the delegate returns a value");

#ifdef HYP_DELEGATE_THREAD_SAFE
        Mutex::Guard guard(m_mutex);
#endif

        const uint32 id = m_id_counter++;

        m_procs.Insert({ id, detail::DelegateHandlerProc<ProcType> { MakeRefCountedPtr<ProcType>(std::move(proc)), calling_thread_id } });

#ifdef HYP_DELEGATE_THREAD_SAFE
        m_num_procs.Increment(1, MemoryOrder::RELEASE);
#endif

        return CreateDelegateHandler(id);
    }

    /*! \brief Remove all bound handlers from the Delegate.
     *  \param thread_safe If true, locks mutex before performing any critical operations to the delegate.
     *  \return The number of handlers removed. */
    virtual int RemoveAll(bool thread_safe = true) override
    {
        const auto ResetImpl = [this]() -> int
        {
            SizeType num_removed = m_procs.Size();
            m_procs.Clear();
            m_id_counter = 0;

            return int(num_removed);
        };

#ifdef HYP_DELEGATE_THREAD_SAFE
        if (thread_safe) {
            m_mutex.Lock();
        }
#endif

        const int num_removed = ResetImpl();
        
#ifdef HYP_DELEGATE_THREAD_SAFE
        m_num_procs.Decrement(uint32(num_removed), MemoryOrder::RELEASE);

        if (thread_safe) {
            m_mutex.Unlock();
        }
#endif

        return num_removed;
    }

    /*! \brief Remove a DelegateHandler from the Delegate
     *  \param handler The DelegateHandler to remove
     *  \return True if the DelegateHandler was removed, false otherwise. */
    virtual bool Remove(const DelegateHandler &handler) override
    {
        if (!handler.IsValid()) {
            return false;
        }

        const bool remove_result = Remove(handler.m_data->id);

        if (remove_result) {
            // now that the handler is removed from the Delegate, invalidate all references to it from the DelegateHandler
            handler.m_data->Reset();

            return true;
        }

        return false;
    }

    /*! \brief Attempt to remove a handler from the Delegate.
     *
     * \param id The ID of the handler to remove.
     * \return True if the handler was removed, false otherwise. */
    virtual bool Remove(uint32 id) override
    {
//         { // remove from detached handlers
// #ifdef HYP_DELEGATE_THREAD_SAFE
//             Mutex::Guard guard(m_detached_handlers_mutex);
// #endif

//             for (auto it = m_detached_handlers.Begin(); it != m_detached_handlers.End();) {
//                 if (it->m_data->id == id) {
//                     it = m_detached_handlers.Erase(it);
//                 } else {
//                     ++it;
//                 }
//             }
//         }

        { // remove bound proc
#ifdef HYP_DELEGATE_THREAD_SAFE
            Mutex::Guard guard(m_mutex);
#endif

            const auto it = m_procs.Find(id);

            if (it == m_procs.End()) {
                return false;
            }

            m_procs.Erase(it);
            
#ifdef HYP_DELEGATE_THREAD_SAFE
            m_num_procs.Decrement(1, MemoryOrder::RELEASE);
#endif
        }

        return true;
    }

    /*! \brief Broadcast a message to all bound handlers.
     *  \note The default return value can be changed by specializing the \ref{hyperion::functional::detail::ProcDefaultReturn} struct.
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref{ReturnType} if no handlers were bound. */
    template <class... ArgTypes>
    ReturnType Broadcast(ArgTypes &&... args)
    {
        if (!AnyBound()) {
            // If no handlers are bound, return a default constructed object or void
            if constexpr (!std::is_void_v<ReturnType>) {
                return detail::ProcDefaultReturn<ReturnType>::Get();
            } else {
                return;
            }
        }

        // @TODO refactor to not use reference counted pointers
        //  - use same array, but just set the procs as invalid when removed
        //  - then, when broadcasting, just skip the invalid procs
        //  - after broadcasting, remove the invalid procs from the array
        Array<detail::DelegateHandlerProc<ProcType>> procs_array;

        {
#ifdef HYP_DELEGATE_THREAD_SAFE
            Mutex::Guard guard(m_mutex);
#endif

            procs_array.Reserve(m_procs.Size());

            for (auto &it : m_procs) {
                procs_array.PushBack(it.second);
            }
        }

        const ThreadID current_thread_id = Threads::CurrentThreadID();

        ValueStorage<ReturnType> result_storage;

        const auto begin = procs_array.Begin();
        const auto end = procs_array.End();

        for (auto it = begin; it != end;) {
            auto current = it;

            if constexpr (!std::is_void_v<ReturnType>) {
                ++it;

                AssertDebugMsg(!current->calling_thread_id.IsValid() || current->calling_thread_id == current_thread_id, "Cannot call a handler on a different thread if the delegate returns a value");
                
                if (it == end) {
                    result_storage.Construct((*current->proc)(args...));
                } else {
                    (*current->proc)(args...);
                }
            } else {
                if (current->calling_thread_id.IsValid() && current->calling_thread_id != current_thread_id) {
                    current->GetCallingThread()->GetScheduler().Enqueue([&proc = *current->proc, args_tuple = Tuple<ArgTypes...>(args...)]()
                    {
                        Apply([&proc]<class... OtherArgs>(OtherArgs &&... args)
                        {
                            return proc(std::forward<OtherArgs>(args)...);
                        }, std::move(args_tuple));
                    }, TaskEnqueueFlags::FIRE_AND_FORGET);
                } else {
                    (*current->proc)(args...);
                }

                ++it;
            }
        }

        if constexpr (!std::is_void_v<ReturnType>) {
            return std::move(result_storage.Get());
        }
    }

    /*! \brief Call operator overload - alias method for Broadcast().
     *  \note The default return value can be changed by specializing the \ref{hyperion::functional::detail::ProcDefaultReturn} struct.
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref{ReturnType} if no handlers were bound. */
    template <class... ArgTypes>
    HYP_FORCE_INLINE ReturnType operator()(ArgTypes &&... args) const
        { return const_cast<Delegate *>(this)->Broadcast(std::forward<ArgTypes>(args)...); }

protected:
    HYP_FORCE_INLINE uint32 NextID()
        { return m_id_counter++; }

    static void RemoveDelegateHandlerCallback(void *delegate, uint32 id)
    {
        Delegate *delegate_casted = static_cast<Delegate *>(delegate);

        delegate_casted->Remove(id);
    }

    static void DetachDelegateHandlerCallback(void *delegate, DelegateHandler &&handler)
    {
        Delegate *delegate_casted = static_cast<Delegate *>(delegate);

        delegate_casted->DetachDelegateHandler(std::move(handler));
    }

    /*! \brief Add a delegate handler to hang around after its DelegateHandler is destructed */
    void DetachDelegateHandler(DelegateHandler &&handler)
    {
#ifdef HYP_DELEGATE_THREAD_SAFE
        Mutex::Guard guard(m_detached_handlers_mutex);
#endif

        m_detached_handlers.PushBack(std::move(handler));
    }

    DelegateHandler CreateDelegateHandler(uint32 id)
    {
        return DelegateHandler(MakeRefCountedPtr<functional::detail::DelegateHandlerData>(
            id,
            this,
            RemoveDelegateHandlerCallback,
            DetachDelegateHandlerCallback
        ));
    }

    FlatMap<uint32, detail::DelegateHandlerProc<ProcType>>  m_procs;

#ifdef HYP_DELEGATE_THREAD_SAFE
    AtomicVar<uint32>                                       m_num_procs;
    Mutex                                                   m_mutex;
#endif

    uint32                                                  m_id_counter;

    Array<DelegateHandler>                                  m_detached_handlers;
    Mutex                                                   m_detached_handlers_mutex;
};

} // namespace functional

using functional::IDelegate;
using functional::Delegate;
using functional::DelegateHandler;
using functional::DelegateHandlerSet;

} // namespace hyperion

#endif