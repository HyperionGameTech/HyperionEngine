/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DELEGATE_HPP
#define HYPERION_DELEGATE_HPP

#include <core/functional/Proc.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/threading/Mutex.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/IDGenerator.hpp>
#include <core/Name.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

namespace functional {

class DelegateHandler;

namespace detail {

struct DelegateHandlerData
{
    uint    id = 0;
    void    *delegate = nullptr;
    void    (*remove_fn)(void *, uint) = nullptr;
    void    (*detach_fn)(void *, DelegateHandler &&delegate_handler) = nullptr;

    HYP_API ~DelegateHandlerData();

    HYP_API void Reset();
    HYP_API void Detach(DelegateHandler &&delegate_handler);

    HYP_FORCE_INLINE
    bool IsValid() const
        { return id != 0 && delegate != nullptr; }

    HYP_FORCE_INLINE
    bool operator==(const DelegateHandlerData &other) const
        { return id == other.id && delegate == other.delegate; }

    HYP_FORCE_INLINE
    bool operator!=(const DelegateHandlerData &other) const
        { return id != other.id || delegate != other.delegate; }

    HYP_FORCE_INLINE
    bool operator<(const DelegateHandlerData &other) const
        { return id < other.id; }
};

template <class ReturnType>
struct DelegateDefaultReturn
{
    static_assert(!std::is_void_v<ReturnType>, "DelegateDefaultReturn should not be used with void return types");
    static_assert(std::is_default_constructible_v<ReturnType>, "DelegateDefaultReturn requires a default constructible type");

    static ReturnType Get()
        { return ReturnType(); }
};

} // namespace detail

/*! \brief Holds a reference to a DelegateHandlerData object.
    When all references to the underlying object are gone, the handler will be removed from the Delegate.
 */
class DelegateHandler
{
public:
    template <class ReturnType, class ... Args>
    friend class Delegate;

    DelegateHandler()                                               = default;

    DelegateHandler(const DelegateHandler &other)                   = default;
    DelegateHandler &operator=(const DelegateHandler &other)        = default;

    DelegateHandler(DelegateHandler &&other) noexcept
        : m_data(std::move(other.m_data))
    {
    }

    DelegateHandler &operator=(DelegateHandler &&other) noexcept    = default;

    ~DelegateHandler()                                              = default;

    HYP_FORCE_INLINE
    bool operator==(const DelegateHandler &other) const
        { return m_data == other.m_data; }

    HYP_FORCE_INLINE
    bool operator!=(const DelegateHandler &other) const
        { return m_data != other.m_data; }

    HYP_FORCE_INLINE
    bool operator<(const DelegateHandler &other) const
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
    HYP_FORCE_INLINE
    bool IsValid() const
        { return m_data != nullptr && m_data->IsValid(); }

    /*! \brief Reset the DelegateHandler to an invalid state. */
    HYP_FORCE_INLINE
    void Reset()
        { m_data.Reset(); }

    /*! \brief Detach the DelegateHandler from the Delegate.
        This will allow the Delegate handler function to remain attached to the delegate upon destruction of this object.
        \note This requires proper management to prevent memory leaks and access of invalid objects, as the lifecycle of the handler will now last
            as long as the Delegate itself. */
    HYP_FORCE_INLINE
    void Detach()
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

/*! \brief Stores a set of DelegateHandlers, intended to hold references to delegates and remove them using RAII */
class DelegateHandlerSet
{
public:
    using Iterator = typename FlatMap<Name, DelegateHandler>::Iterator;
    using ConstIterator = typename FlatMap<Name, DelegateHandler>::ConstIterator;

    HYP_FORCE_INLINE
    DelegateHandlerSet &Add(const DelegateHandler &delegate_handler)
    {
        m_delegate_handlers.Insert({ Name::Unique(), delegate_handler });
        return *this;
    }

    HYP_FORCE_INLINE
    DelegateHandlerSet &Add(Name name, const DelegateHandler &delegate_handler)
    {
        m_delegate_handlers.Insert({ name, delegate_handler });
        return *this;
    }

    HYP_FORCE_INLINE
    bool Remove(Name name)
    {
        auto it = m_delegate_handlers.Find(name);

        if (it == m_delegate_handlers.End()) {
            return false;
        }

        m_delegate_handlers.Erase(it);

        return true;
    }

    HYP_FORCE_INLINE
    bool Remove(ConstIterator it)
    {
        if (it == m_delegate_handlers.End()) {
            return false;
        }

        m_delegate_handlers.Erase(it);

        return true;
    }

    HYP_FORCE_INLINE
    Iterator Find(Name name)
        { return m_delegate_handlers.Find(name); }

    HYP_FORCE_INLINE
    ConstIterator Find(Name name) const
        { return m_delegate_handlers.Find(name); }

    HYP_FORCE_INLINE
    bool Contains(Name name) const
        { return m_delegate_handlers.Contains(name); }

    HYP_DEF_STL_BEGIN_END(
        m_delegate_handlers.Begin(),
        m_delegate_handlers.End()
    )

private:
    FlatMap<Name, DelegateHandler>  m_delegate_handlers;
};

/*! \brief A Delegate object that can be used to bind handler functions to be called when a broadcast is sent.
 *  Handlers can be bound as strong or weak references, and adding them is thread safe.
 *
 *  \tparam ReturnType The return type of the handler functions.
 *  \tparam Args The argument types of the handler functions. */
template <class ReturnType, class ... Args>
class Delegate
{
    using ProcType = Proc<ReturnType, Args...>;

public:
    Delegate() = default;

    Delegate(const Delegate &other) = delete;
    Delegate &operator=(const Delegate &other) = delete;

    Delegate(Delegate &&other) noexcept
        : m_procs(std::move(other.m_procs)),
          m_detached_handlers(std::move(other.m_detached_handlers)),
          m_id_generator(std::move(other.m_id_generator))
    {
    }

    Delegate &operator=(Delegate &&other) noexcept = delete;

    ~Delegate()
    {
        m_detached_handlers.Clear();
    }

    /*! \brief Bind a Proc<> to the Delegate.
     * \note The handler will be removed when the last reference to the returned DelegateHandler is removed.
     *  This makes it easy to manage resource cleanup, as you can store the DelegateHandler as a class member and when the object is destroyed, the handler will be removed from the Delegate.
     *
     * \param proc The Proc to bind.
     * \return  A reference counted DelegateHandler object that can be used to remove the handler from the Delegate. */
    DelegateHandler Bind(ProcType &&proc)
    {
        const uint id = m_id_generator.NextID();

        Mutex::Guard guard(m_mutex);
        m_procs.Insert({ id, RC<ProcType>::Construct(std::move(proc)) });

        return CreateDelegateHandler(id);
    }

    /*! \brief Remove all bound handlers from the Delegate.
     *  \return The number of handlers removed. */
    int RemoveAll()
    {
        m_mutex.Lock();

        const int num_removed = m_procs.Size();

        m_procs.Clear();

        m_mutex.Unlock();

        m_id_generator.Reset();

        return num_removed;
    }

    /*! \brief Remove a DelegateHandler from the Delegate
     *  \param handler The DelegateHandler to remove
     *  \return True if the DelegateHandler was removed, false otherwise. */
    bool Remove(const DelegateHandler &handler)
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
    bool Remove(uint id)
    {
        m_mutex.Lock();

        const auto it = m_procs.Find(id);

        if (it == m_procs.End()) {
            m_mutex.Unlock();

            return false;
        }

        m_procs.Erase(it);

        m_mutex.Unlock();

        m_id_generator.FreeID(id);

        return true;
    }

    /*! \brief Broadcast a message to all bound handlers. Returns the first result of the handlers, or a default constructed \ref{ReturnType} if no handlers were bound.
     *  \note The default return value can be changed by specializing the \ref{hyperion::functional::detail::DelegateDefaultReturn} struct.
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The first result of the handlers, or a default constructed \ref{ReturnType} if no handlers were bound. */
    template <class ... ArgTypes>
    ReturnType Broadcast(ArgTypes &&... args)
    {
        Array<RC<ProcType>> procs_array;

        {
            Mutex::Guard guard(m_mutex);

            // If no handlers are bound, return a default constructed object or void
            if (m_procs.Empty()) {
                if constexpr (!std::is_void_v<ReturnType>) {
                    // default construct the return object
                    return detail::DelegateDefaultReturn<ReturnType>::Get();
                } else {
                    // void return type
                    return;
                }
            }

            procs_array.Reserve(m_procs.Size());

            for (auto &it : m_procs) {
                procs_array.PushBack(it.second);
            }
        }

        ValueStorage<ReturnType> result_storage;

        const auto begin = procs_array.Begin();
        const auto end = procs_array.End();

        for (auto it = begin; it != end; ++it) {
            if constexpr (!std::is_void_v<ReturnType>) {
                if (it == begin) {
                    result_storage.Construct((**it)(args...));

                    continue;
                }
            }

            (**it)(args...);
        }

        if constexpr (!std::is_void_v<ReturnType>) {
            return std::move(result_storage.Get());
        }
    }

    /*! \brief Call operator overload - alias method for Broadcast(). Returns the first result of the handlers, or a default constructed \ref{ReturnType} if no handlers were bound.
     *  \note The default return value can be changed by specializing the \ref{hyperion::functional::detail::DelegateDefaultReturn} struct.
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The first result of the handlers, or a default constructed \ref{ReturnType} if no handlers were bound. */
    template <class ... ArgTypes>
    HYP_FORCE_INLINE
    ReturnType operator()(ArgTypes &&... args)
        { return Broadcast(std::forward<ArgTypes>(args)...); }

private:
    static void RemoveDelegateHandlerCallback(void *delegate, uint id)
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
        Mutex::Guard guard(m_detached_handlers_mutex);

        m_detached_handlers.PushBack(std::move(handler));
    }

    DelegateHandler CreateDelegateHandler(uint id)
    {
        return DelegateHandler(RC<functional::detail::DelegateHandlerData>(new functional::detail::DelegateHandlerData {
            id,
            this,
            RemoveDelegateHandlerCallback,
            DetachDelegateHandlerCallback
        }));
    }

    FlatMap<uint, RC<ProcType>> m_procs;
    Mutex                       m_mutex;

    Array<DelegateHandler>      m_detached_handlers;
    Mutex                       m_detached_handlers_mutex;

    IDGenerator                 m_id_generator;
};
} // namespace functional

using functional::Delegate;
using functional::DelegateHandler;
using functional::DelegateHandlerSet;

} // namespace hyperion

#endif