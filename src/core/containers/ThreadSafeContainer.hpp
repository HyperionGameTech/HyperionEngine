/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>
#include <core/threading/Thread.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/threading/Threads.hpp>

#include <core/Types.hpp>

#include <mutex>
#include <atomic>

namespace hyperion {

template <class T>
class ThreadSafeContainer
{
public:
    using Iterator = typename Array<Handle<T>>::Iterator;
    using ConstIterator = typename Array<Handle<T>>::ConstIterator;

    ThreadSafeContainer(const ThreadId& ownerThread)
        : m_ownerThread(ownerThread)
    {
    }

    ThreadSafeContainer(const ThreadSafeContainer& other) = delete;
    ThreadSafeContainer& operator=(const ThreadSafeContainer& other) = delete;
    ThreadSafeContainer(ThreadSafeContainer&& other) noexcept = delete;
    ThreadSafeContainer& operator=(ThreadSafeContainer&& other) noexcept = delete;

    ~ThreadSafeContainer()
    {
        Clear(false);
    }

    void Add(const Handle<T>& item)
    {
        if (!item)
        {
            return;
        }

        std::lock_guard guard(m_updateMutex);

        m_itemsPendingRemoval.Erase(item->Id());
        m_itemsPendingAddition.PushBack(item);

        m_updatesPending.store(true);
    }

    void Add(Handle<T>&& item)
    {
        if (!item)
        {
            return;
        }

        std::lock_guard guard(m_updateMutex);

        m_itemsPendingRemoval.Erase(item->Id());
        m_itemsPendingAddition.PushBack(std::move(item));

        m_updatesPending.store(true);
    }

    void Remove(ObjId<T> id)
    {
        if (!id)
        {
            return;
        }

        std::lock_guard guard(m_updateMutex);

        auto it = m_itemsPendingAddition.FindIf([&id](const auto& item)
            {
                return item->Id() == id;
            });

        if (it != m_itemsPendingAddition.End())
        {
            m_itemsPendingAddition.Erase(it);
        }

        m_itemsPendingRemoval.PushBack(id);

        m_updatesPending.store(true);
    }

    bool HasUpdatesPending() const
    {
        return m_updatesPending.load();
    }

    /*! \brief Adds and removes all pending items to be added or removed.
        Only call from the owner thread.*/
    void UpdateItems()
    {
        Threads::AssertOnThread(m_ownerThread);

        m_updateMutex.lock();

        auto pendingRemoval = std::move(m_itemsPendingRemoval);
        auto pendingAddition = std::move(m_itemsPendingAddition);

        m_updatesPending.store(false);

        m_updateMutex.unlock();

        // do most of the work outside of the lock.
        // due to the std::move() usage, m_itemsPendingRemoval and m_itemsPendingAddition
        // will be cleared, we've taken all items and will work off them here.

        while (pendingRemoval.Any())
        {
            auto front = pendingRemoval.PopFront();

            auto it = m_ownedItems.FindIf([&front](const auto& item)
                {
                    return item->Id() == front;
                });

            if (it != m_ownedItems.End())
            {
                m_ownedItems.Erase(it);
            }
        }

        while (pendingAddition.Any())
        {
            auto front = pendingAddition.PopFront();

            auto it = m_ownedItems.Find(front);

            if (it == m_ownedItems.End())
            {
                InitObject(front);

                m_ownedItems.PushBack(std::move(front));
            }
        }
    }

    void Clear(bool checkThreadId = true)
    {
        if (checkThreadId)
        {
            Threads::AssertOnThread(m_ownerThread);
        }

        if (HasUpdatesPending())
        {
            std::lock_guard guard(m_updateMutex);

            m_itemsPendingRemoval.Clear();
            m_itemsPendingAddition.Clear();
        }

        m_ownedItems.Clear();
    }

    /*! \note Only use from the owner thread! */
    Array<Handle<T>>& GetItems()
    {
        return m_ownedItems;
    }

    /*! \note Only use from the owner thread! */
    const Array<Handle<T>>& GetItems() const
    {
        return m_ownedItems;
    }

    /*! \note Only iterate it on the owner thread! */
    HYP_DEF_STL_ITERATOR(m_ownedItems);

private:
    ThreadId m_ownerThread;
    Array<Handle<T>> m_ownedItems;
    Array<Handle<T>> m_itemsPendingAddition;
    Array<ObjId<T>> m_itemsPendingRemoval;
    std::atomic_bool m_updatesPending;
    std::mutex m_updateMutex;
};

} // namespace hyperion
