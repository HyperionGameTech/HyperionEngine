/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DELEGATE_HPP
#define HYPERION_DELEGATE_HPP

#include <core/functional/Proc.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>
#include <core/threading/Mutex.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/IDCreator.hpp>

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

    DelegateHandler(DelegateHandler &&other) noexcept               = default;
    DelegateHandler &operator=(DelegateHandler &&other) noexcept    = default;

    ~DelegateHandler()                                              = default;

    HYP_FORCE_INLINE
    bool operator==(const DelegateHandler &other) const
        { return m_data == other.m_data; }

    HYP_FORCE_INLINE
    bool operator!=(const DelegateHandler &other) const
        { return m_data != other.m_data; }

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

/*! \brief A Delegate object that can be used to bind handler functions to be called when a broadcast is sent.
 *  Handlers can be bound as strong or weak references, and adding them is thread safe.
 *
 *  \tparam ReturnType The return type of the handler functions.
 *  \tparam Args The argument types of the handler functions. */
template <class ReturnType, class ... Args>
class Delegate
{
    using ProcType = Proc<ReturnType, Args...>;

    static constexpr bool returns_boolean = std::is_same_v<ReturnType, bool>;

public:
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
        m_procs.Insert(id, std::move(proc));

        return CreateDelegateHandler(id);
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
        if (!m_procs.Erase(id)) {
            m_mutex.Unlock();
            return false;
        }

        m_mutex.Unlock();

        m_id_generator.FreeID(id);

        return true;
    }

    /*! \brief Broadcast a message to all bound handlers.

     *  \tparam ArgTypes The argument types to pass to the handlers.

     *  \param args The arguments to pass to the handlers.
     *  \return If \ref{ReturnType} is bool, the result will return true if any handler returned true.
     *  Otherwise, the result will always be true, indicating that all handlers were called. */
    template <class ... ArgTypes>
    bool Broadcast(ArgTypes &&... args)
    {
        Mutex::Guard guard(m_mutex);

        if (m_procs.Empty()) {
            return false;
        }

        if constexpr (returns_boolean) {
            bool result = false;

            for (auto &it : m_procs) {
                result |= it.second((args)...);
            }

            return result;
        } else {
            for (auto &it : m_procs) {
                it.second((args)...);
            }

            return true;
        }
    }

    /*! \brief Call operator overload - alias method for Broadcast().
     *
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *
     *  \param args The arguments to pass to the handlers.
     *  \return If \ref{ReturnType} is bool, the result will return true if any handler returned true.
     *  Otherwise, the result will always be true, indicating that all handlers were called. */
    template <class ... ArgTypes>
    HYP_FORCE_INLINE
    bool operator()(ArgTypes &&... args)
        { return Broadcast(std::forward<ArgTypes>(args)...); }

private:
    static void RemoveDelegateHandlerCallback(void *delegate, uint id)
    {
        Delegate *delegate_casted = static_cast<Delegate *>(delegate);

        Mutex::Guard guard(delegate_casted->m_mutex);
        if (!delegate_casted->m_procs.Erase(id)) {
            return;
        }

        delegate_casted->m_id_generator.FreeID(id);
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

    HashMap<uint, ProcType>         m_procs;
    Mutex                           m_mutex;

    Array<DelegateHandler>          m_detached_handlers;
    Mutex                           m_detached_handlers_mutex;

    IDCreator<>                     m_id_generator;
};
} // namespace functional

using functional::Delegate;
using functional::DelegateHandler;

} // namespace hyperion

#endif