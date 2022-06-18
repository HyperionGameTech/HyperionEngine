#ifndef HYPERION_V2_CONTROLLER_H
#define HYPERION_V2_CONTROLLER_H

#include <rendering/base.h>
#include "../game_counter.h"

#include <core/lib/flat_map.h>

#include <atomic>
#include <unordered_map>
#include <type_traits>

namespace hyperion::v2 {

using ControllerId = uint32_t;

class Node;
class Engine;

class Controller {
    friend class Node;

public:
    Controller(const char *name);
    Controller(const Controller &other) = delete;
    Controller &operator=(const Controller &other) = delete;
    virtual ~Controller();

    const char *GetName() const { return m_name; }
    Node *GetParent() const     { return m_parent; }

protected:
    virtual void OnAdded() = 0;
    virtual void OnRemoved() = 0;
    virtual void OnUpdate(GameCounter::TickUnit delta) = 0;

    virtual void OnDescendentAdded(Node *descendent) {}
    virtual void OnDescendentRemoved(Node *descendent) {}

private:
    char *m_name;
    Node *m_parent;
};

class ControllerSet {
    using Map = FlatMap<ControllerId, std::unique_ptr<Controller>>;

    static std::atomic<ControllerId> controller_id_counter;

    template <class T>
    inline static ControllerId GetControllerId()
    {
        static_assert(std::is_base_of_v<Controller, T>, "Object must be a derived class of Controller");

        static const ControllerId id = ++controller_id_counter;

        return id;
    }

public:
    using Iterator      = Map::Iterator;
    using ConstIterator = Map::ConstIterator;

    ControllerSet() = default;
    ControllerSet(const ControllerSet &other) = delete;
    ControllerSet &operator=(const ControllerSet &other) = delete;

    ControllerSet(ControllerSet &&other) noexcept
        : m_map(std::move(other.m_map))
    {
    }

    ControllerSet &operator=(ControllerSet &&other) noexcept
    {
        m_map = std::move(other.m_map);

        return *this;
    }

    ~ControllerSet() = default;
    
    template <class T>
    void Set(std::unique_ptr<T> &&controller)
    {
        AssertThrow(controller != nullptr);

        const auto id = GetControllerId<T>();

        m_map[id] = std::move(controller);
    }

    template <class T>
    T *Get() const
    {
        const auto id = GetControllerId<T>();

        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return nullptr;
        }

        return static_cast<T *>(it->second.get());
    }

    template <class T>
    bool Has() const
    {
        const auto id = GetControllerId<T>();

        return m_map.Find(id) != m_map.End();
    }
    
    template <class T>
    bool Remove()
    {
        const auto id = GetControllerId<T>();

        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return false;
        }

        m_map.Erase(it);

        return true;
    }

    HYP_DEF_STL_BEGIN_END(m_map.begin(), m_map.end());

private:
    Map m_map;
};

} // namespace hyperion::v2

#endif