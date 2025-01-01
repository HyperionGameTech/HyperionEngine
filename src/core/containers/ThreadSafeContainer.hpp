/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_THREAD_SAFE_CONTAINER_HPP
#define HYPERION_THREAD_SAFE_CONTAINER_HPP

#include <core/threading/Thread.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/threading/Threads.hpp>

#include <Types.hpp>

#include <mutex>
#include <atomic>

namespace hyperion {

template <class T>
class ThreadSafeContainer
{
public:
    using Iterator = typename Array<T>::Iterator;
    using ConstIterator = typename Array<T>::ConstIterator;

    ThreadSafeContainer(ThreadName owner_thread)
        : m_owner_thread(owner_thread)
    {
    }

    ThreadSafeContainer(const ThreadSafeContainer &other)                   = delete;
    ThreadSafeContainer &operator=(const ThreadSafeContainer &other)        = delete;
    ThreadSafeContainer(ThreadSafeContainer &&other) noexcept               = delete;
    ThreadSafeContainer &operator=(ThreadSafeContainer &&other) noexcept    = delete;

    ~ThreadSafeContainer()
    {
        Clear(false);
    }

    void Add(const T &item)
    {
        std::lock_guard guard(m_update_mutex);

        m_items_pending_removal.Erase(&item);
        m_items_pending_addition.PushBack(item);

        m_updates_pending.store(true);
    }

    void Add(T &&item)
    {
        std::lock_guard guard(m_update_mutex);

        m_items_pending_removal.Erase(&item);
        m_items_pending_addition.PushBack(std::move(item));

        m_updates_pending.store(true);
    }

    void Remove(const T &element)
    {
        std::lock_guard guard(m_update_mutex);

        auto it = m_items_pending_addition.FindIf([&element](const auto &item)
        {
            return item == element;
        });

        if (it != m_items_pending_addition.End()) {
            m_items_pending_addition.Erase(it);
        }

        m_items_pending_removal.PushBack(&element);

        m_updates_pending.store(true);
    }

    bool HasUpdatesPending() const
        { return m_updates_pending.load(); }

    /*! \brief Adds and removes all pending items to be added or removed.
        Only call from the owner thread.*/
    void UpdateItems()
    {
        Threads::AssertOnThread(m_owner_thread);

        m_update_mutex.lock();

        Array<const T *> pending_removal = std::move(m_items_pending_removal);
        Array<T> pending_addition = std::move(m_items_pending_addition);

        m_updates_pending.store(false);

        m_update_mutex.unlock();

        // do most of the work outside of the lock.
        // due to the std::move() usage, m_items_pending_removal and m_items_pending_addition
        // will be cleared, we've taken all items and will work off them here.

        while (pending_removal.Any()) {
            const T *front = pending_removal.PopFront();

            auto it = m_owned_items.FindIf([front](const auto &item)
            {
                return &item == front;
            });

            if (it != m_owned_items.End()) {
                m_owned_items.Erase(it);
            }
        }

        while (pending_addition.Any()) {
            T front = pending_addition.PopFront();

            auto it = m_owned_items.Find(front);

            if (it == m_owned_items.End()) {
                m_owned_items.PushBack(std::move(front));
            }
        }
    }

    void Clear(bool check_thread_id = true)
    {
        if (check_thread_id) {
            Threads::AssertOnThread(m_owner_thread);
        }

        if (HasUpdatesPending()) {
            std::lock_guard guard(m_update_mutex);

            m_items_pending_removal.Clear();
            m_items_pending_addition.Clear();
        }

        m_owned_items.Clear();
    }

    /*! \note Only use from the owner thread! */
    Array<T> &GetItems()
        { return m_owned_items; }
    
    /*! \note Only use from the owner thread! */
    const Array<T> &GetItems() const
        { return m_owned_items; }

    /*! \note Only iterate it on the owner thread! */
    HYP_DEF_STL_ITERATOR(m_owned_items);

private:
    ThreadName          m_owner_thread;
    Array<T>            m_owned_items;
    Array<T>            m_items_pending_addition;
    Array<const T *>    m_items_pending_removal;
    std::atomic_bool    m_updates_pending;
    std::mutex          m_update_mutex;
};

} // namespace hyperion

#endif