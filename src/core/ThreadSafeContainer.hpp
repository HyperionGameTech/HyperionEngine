#ifndef HYPERION_V2_LIB_THREAD_SAFE_CONTAINER_HPP
#define HYPERION_V2_LIB_THREAD_SAFE_CONTAINER_HPP

#include <core/OpaqueHandle.hpp>
#include <core/Thread.hpp>
#include <core/lib/DynArray.hpp>

#include <Threads.hpp>

#include <Types.hpp>

#include <mutex>
#include <atomic>

namespace hyperion {

using namespace v2;

template <class T>
class ThreadSafeContainer
{
public:
    using Iterator = typename Array<Handle<T>>::Iterator;
    using ConstIterator = typename Array<Handle<T>>::ConstIterator;

    ThreadSafeContainer(ThreadName owner_thread)
        : m_owner_thread(owner_thread)
    {
    }

    ThreadSafeContainer(const ThreadSafeContainer &other) = delete;
    ThreadSafeContainer &operator=(const ThreadSafeContainer &other) = delete;
    ThreadSafeContainer(ThreadSafeContainer &&other) noexcept = delete;
    ThreadSafeContainer &operator=(ThreadSafeContainer &&other) noexcept = delete;

    ~ThreadSafeContainer()
    {
        Clear(false);
    }

    void Add(const Handle<T> &item)
    {
        if (!item) {
            return;
        }

        std::lock_guard guard(m_update_mutex);

        m_items_pending_removal.Erase(item->GetID());
        m_items_pending_addition.PushBack(item);

        m_updates_pending.store(true);
    }

    void Add(Handle<T> &&item)
    {
        if (!item) {
            return;
        }

        std::lock_guard guard(m_update_mutex);

        m_items_pending_removal.Erase(item->GetID());
        m_items_pending_addition.PushBack(std::move(item));

        m_updates_pending.store(true);
    }

    void Remove(typename Handle<T>::ID id)
    {
        if (!id) {
            return;
        }

        std::lock_guard guard(m_update_mutex);

        auto it = m_items_pending_addition.FindIf([&id](const auto &item) {
            return item->GetID() == id;
        });

        if (it != m_items_pending_addition.End()) {
            m_items_pending_addition.Erase(it);
        }

        m_items_pending_removal.PushBack(id);

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

        auto pending_removal = std::move(m_items_pending_removal);
        auto pending_addition = std::move(m_items_pending_addition);

        m_updates_pending.store(false);

        m_update_mutex.unlock();

        // do most of the work outside of the lock.
        // due to the std::move() usage, m_items_pending_removal and m_items_pending_addition
        // will be cleared, we've taken all items and will work off them here.

        while (pending_removal.Any()) {
            auto front = pending_removal.PopFront();

            auto it = m_owned_items.FindIf([&front](const auto &item) {
                return item->GetID() == front;
            });

            if (it != m_owned_items.End()) {
                m_owned_items.Erase(it);
            }
        }

        while (pending_addition.Any()) {
            auto front = pending_addition.PopFront();

            auto it = m_owned_items.FindIf([&front](const auto &item) {
                return item->GetID() == front;
            });

            if (it == m_owned_items.End()) {
                // Engine::Get()->InitObject(front);

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

    /*! Only use from the owner thread! */
    Array<Handle<T>> &GetItems()
        { return m_owned_items; }
    
    /*! Only use from the owner thread! */
    const Array<Handle<T>> &GetItems() const
        { return m_owned_items; }

    /*! Only iterate it on the owner thread! */
    HYP_DEF_STL_ITERATOR(m_owned_items);

private:
    ThreadName m_owner_thread;
    Array<Handle<T>> m_owned_items;
    Array<Handle<T>> m_items_pending_addition;
    Array<typename Handle<T>::ID> m_items_pending_removal;
    std::atomic_bool m_updates_pending;
    std::mutex m_update_mutex;
};

} // namespace hyperion

#endif