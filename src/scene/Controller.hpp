#ifndef HYPERION_V2_CONTROLLER_H
#define HYPERION_V2_CONTROLLER_H

#include <core/Core.hpp>
#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <math/Transform.hpp>
#include <script/Script.hpp>
#include <GameCounter.hpp>
#include <Types.hpp>

#include <atomic>
#include <unordered_map>
#include <type_traits>

namespace hyperion::v2 {

using ControllerID = UInt;

class Node;
class Entity;
class Scene;
class Engine;

class Controller
{
    friend class Entity;

protected:
    enum ScriptMethods
    {
        SCRIPT_METHOD_ON_ADDED,
        SCRIPT_METHOD_ON_REMOVED,
        SCRIPT_METHOD_ON_TICK,

        SCRIPT_METHOD_0,
        SCRIPT_METHOD_1,
        SCRIPT_METHOD_2,
        SCRIPT_METHOD_3,
        SCRIPT_METHOD_4,
        SCRIPT_METHOD_5,
        SCRIPT_METHOD_6,
        SCRIPT_METHOD_7,
        SCRIPT_METHOD_8,
        SCRIPT_METHOD_9,
        SCRIPT_METHOD_10,
        SCRIPT_METHOD_11,

        SCRIPT_METHOD_MAX
    };

public:
    Controller(const String &name, bool receives_update = true);
    Controller(const Controller &other) = delete;
    Controller &operator=(const Controller &other) = delete;

    // Controller(Controller &&other) noexcept;
    // Controller &operator=(Controller &&other) noexcept;

    virtual ~Controller();

    const String &GetName() const
        { return m_name; }

    Entity *GetOwner() const
        { return m_owner; }

    bool ReceivesUpdate() const
        { return m_receives_update; }

    bool HasScript() const
        { return m_script.IsValid(); }

    bool IsScriptValid() const
        { return m_script_valid; }

    void SetScript(const Handle<Script> &script);
    void SetScript(Handle<Script> &&script);

    virtual void OnAdded();
    virtual void OnRemoved();
    virtual void OnUpdate(GameCounter::TickUnit delta);

    virtual void OnTransformUpdate(const Transform &transform) {}

    virtual void OnDetachedFromNode(Node *node) {}
    virtual void OnAttachedToNode(Node *node) {}

    virtual void OnDetachedFromScene(Scene *scene) {}
    virtual void OnAttachedToScene(Scene *scene) {}

protected:
    bool CreateScriptedObjects();
    virtual bool CreateScriptedMethods();
    
    Script::ObjectHandle m_self_object;
    FixedArray<Script::FunctionHandle, SCRIPT_METHOD_MAX> m_script_methods;
    Handle<Script> m_script;
    bool m_script_valid;

private:
    String m_name;
    Entity *m_owner;
    bool m_receives_update;
};

class ControllerSet
{
    using Map = FlatMap<ControllerID, UniquePtr<Controller>>;

    static std::atomic<ControllerID> controller_id_counter;

    template <class T>
    static ControllerID GetControllerID()
    {
        static_assert(std::is_base_of_v<Controller, T>, "Object must be a derived class of Controller");

        static const ControllerID id = ++controller_id_counter;

        return id;
    }

public:
    using Iterator = typename Map::Iterator;
    using ConstIterator = typename Map::ConstIterator;

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
    void Set(UniquePtr<T> &&controller)
    {
        AssertThrow(controller != nullptr);

        const auto id = GetControllerID<T>();

        m_map[id] = std::move(controller);
    }

    template <class T>
    T *Get() const
    {
        const auto id = GetControllerID<T>();

        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return nullptr;
        }

        return static_cast<T *>(it->second.Get());
    }

    template <class T>
    bool Has() const
    {
        const auto id = GetControllerID<T>();

        return m_map.Find(id) != m_map.End();
    }
    
    template <class T>
    bool Remove()
    {
        const auto id = GetControllerID<T>();

        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return false;
        }

        m_map.Erase(it);

        return true;
    }

    void Clear()
    {
        m_map.Clear();
    }

    HYP_DEF_STL_BEGIN_END(m_map.begin(), m_map.end());

private:
    Map m_map;
};

} // namespace hyperion::v2

#endif