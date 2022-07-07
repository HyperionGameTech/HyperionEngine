#ifndef HYPERION_V2_CORE_OBSERVER_H
#define HYPERION_V2_CORE_OBSERVER_H

#include <functional>
#include <vector>
#include <mutex>

namespace hyperion::v2 {


template <class T>
struct ObserverRef;

template <class T, class Observer>
class Notifier {
public:
    using GetCurrentItemsFunction = std::function<std::vector<std::pair<T *, size_t>>()>;

    Notifier(GetCurrentItemsFunction &&get_current_items)
        : m_get_current_items(std::move(get_current_items)),
          m_id_counter(0)
    {
    }

    ~Notifier()
    {
        for (const auto &it : m_get_current_items()) {
            for (auto &observer : m_observers) {
                observer.m_on_items_removed(it.first, it.second);
                observer.Reset();
            }
        }

        m_observers.clear();
    }

    ObserverRef<T> Add(Observer &&observer)
    {
        // std::lock_guard guard(m_mutex);

        observer.m_data = {++m_id_counter, this};

        /* call with all current items. */
        for (const auto &it : m_get_current_items()) {
            observer.m_on_items_added(it.first, it.second);
        }

        m_observers.push_back(std::move(observer));

        const auto &data = m_observers.back().m_data;

        return ObserverRef<T>(data.value, data.notifier);
    }

    bool Remove(uint32_t value)
    {
        // std::lock_guard guard(m_mutex);

        auto it = std::find_if(
            m_observers.begin(),
            m_observers.end(),
            [&](const auto &item) {
                return item.m_data.value == value;
            }
        );

        if (it == m_observers.end()) {
            return false;
        }
        
        for (const auto &items : m_get_current_items()) {
            it->m_on_items_removed(items.first, items.second);
        }

        it->Reset();

        m_observers.erase(it);

        return true;
    }

    bool Remove(const ObserverRef<T> &ref)
    {
        return Remove(ref.data.value);
    }

    void ItemAdded(T &item)
    {
        // std::lock_guard guard(m_mutex);

        for (auto &observer : m_observers) {
            observer.m_on_items_added(&item, 1);
        }
    }

    void ItemRemoved(T &item)
    {
        std::lock_guard guard(m_mutex);

        for (auto &observer : m_observers) {
            observer.m_on_items_removed(&item, 1);
        }
    }

    void ItemsAdded(T *ptr, size_t count)
    {
        // std::lock_guard guard(m_mutex);

        for (auto &observer : m_observers) {
            observer.m_on_items_added(ptr, count);
        }
    }

    void ItemsRemoved(T *ptr, size_t count)
    {
        // std::lock_guard guard(m_mutex);

        for (auto &observer : m_observers) {
            observer.m_on_items_removed(ptr, count);
        }
    }

private:
    std::vector<Observer>   m_observers;
    GetCurrentItemsFunction m_get_current_items;
    uint32_t                m_id_counter;
    std::mutex              m_mutex;
};

template <class ObserverNotifier>
struct ObserverRefData {
    uint32_t          value    = 0;
    ObserverNotifier *notifier = nullptr;
};

template <class T>
class Observer {
public:
    using Function = std::function<void(T *ptr, size_t count)>;

    Observer(Function &&on_items_added, Function &&on_items_removed)
        : m_on_items_added(std::move(on_items_added)),
          m_on_items_removed(std::move(on_items_removed))
    {
    }

    Observer(const Observer &other) = delete;
    Observer &operator=(const Observer &other) = delete;

    Observer(Observer &&other) noexcept
        : m_on_items_added(std::move(other.m_on_items_added)),
          m_on_items_removed(std::move(other.m_on_items_removed)),
          m_data(other.m_data)
    {
        other.m_on_items_added   = nullptr;
        other.m_on_items_removed = nullptr;
        other.Reset();
    }

    Observer &operator=(Observer &&other) noexcept
    {
        m_on_items_added   = std::move(other.m_on_items_added);
        m_on_items_removed = std::move(other.m_on_items_removed);
        m_data             = other.m_data;

        other.m_on_items_added   = nullptr;
        other.m_on_items_removed = nullptr;
        other.Reset();

        return *this;
    }

    ~Observer()
    {
        Remove();
    }

    bool Remove()
    {
        if (m_data.notifier != nullptr) {
            if (m_data.notifier->Remove(m_data.value)) {
                return true;
            }
        }

        return false;
    }

    void Reset()
    {
        m_data = {0, nullptr};
    }
    
    Function m_on_items_added,
             m_on_items_removed;

    ObserverRefData<Notifier<T, Observer>> m_data;
    
};

template <class T>
using ObserverNotifier = Notifier<T, Observer<T>>;

template <class T>
struct ObserverRef {
    struct Count {
        uint32_t count = 0;
    } *count_ptr = nullptr;

    ObserverRefData<ObserverNotifier<T>> data;

    /*ObserverRef()
        : value(0),
          notifier(nullptr),
          count_ptr(new Count{1})
    {
    }*/

    ObserverRef(uint32_t value, ObserverNotifier<T> *notifier)
        : data{value, notifier},
          count_ptr(new Count{1})
    {
    }

    ObserverRef(const ObserverRef &other)
        : data{other.data.value, other.data.notifier},
          count_ptr(other.count_ptr)
    {
        if (count_ptr != nullptr) {
            ++count_ptr->count;
        }
    }

    ObserverRef &operator=(const ObserverRef &other)
    {
        if (this == &other) {
            return *this;
        }

        Remove();

        data      = other.data;
        count_ptr = other.count_ptr;

        if (count_ptr != nullptr) {
            ++count_ptr->count;
        }

        return *this;
    }

    ObserverRef(ObserverRef &&other) noexcept
        : data{other.data.value, other.data.notifier},
          count_ptr(other.count_ptr)
    {
        other.data      = {0, nullptr};
        other.count_ptr = nullptr;
    }

    ObserverRef &operator=(ObserverRef &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        std::swap(data.value, other.data.value);
        std::swap(data.notifier, other.data.notifier);
        std::swap(count_ptr, other.count_ptr);

        other.Remove();

        return *this;
    }


    ~ObserverRef()
    {
        Remove();
    }

    operator bool() const { return data.value != 0 && data.notifier != nullptr && count_ptr != nullptr; }

    void Reset()
    {
        if (count_ptr != nullptr) {
            --count_ptr->count;

            if (count_ptr->count == 0) {
                delete count_ptr;
            }
        }

        count_ptr = nullptr;
        data      = {0, nullptr};
    }

    bool Remove()
    {
        if (data.notifier != nullptr) {
            if (data.notifier->Remove(*this)) {
                return true;
            }
        }
        
        Reset(); // force removal

        return false;
    }
};

} // namespace hyperion::v2

#endif