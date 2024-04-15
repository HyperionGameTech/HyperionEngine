/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_V2_CONTROLLER_H
#define HYPERION_V2_CONTROLLER_H

#include <core/Core.hpp>
#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <math/Transform.hpp>
#include <asset/serialization/fbom/FBOMResult.hpp>
#include <GameCounter.hpp>
#include <Types.hpp>

#include <atomic>
#include <unordered_map>
#include <type_traits>

//#define HYP_CONTROLLER_SERIALIZATION_METHDOS_PURE_VIRTUAL

namespace hyperion::v2 {

namespace fbom {
class FBOMObject;
} // namespace fbom

using ControllerID = uint;

class Node;
class Entity;
class Scene;
class Engine;
class Controller;

struct ControllerSerializationWrapper
{
    TypeID                  type_id;
    UniquePtr<Controller>   controller;
};

template <class T>
struct AssetLoadResultWrapper;

template <>
struct AssetLoadResultWrapper<Controller>
{
    using type = ControllerSerializationWrapper;
};

class Controller
{
    friend class Entity;

protected:
public:
    Controller(bool receives_update = true);
    Controller(const Controller &other) = delete;
    Controller &operator=(const Controller &other) = delete;

    // Controller(Controller &&other) noexcept;
    // Controller &operator=(Controller &&other) noexcept;

    virtual ~Controller();

    /*! \brief Get the owner of this controller.
     *
     *  \return The pointer to the Entity owner of this controller.
     */
    Entity *GetOwner() const
        { return m_owner; }

    /*! \brief Set the owner of this controller. Only to be used by the engine, internally.
     *
     *  \param owner The new owner of this controller.
     */
    void SetOwner(Entity *owner)
        { m_owner = owner; }

    bool ReceivesUpdate() const
        { return m_receives_update; }

    virtual void OnAdded();
    virtual void OnRemoved();
    virtual void OnUpdate(GameCounter::TickUnit delta);

    virtual void OnTransformUpdate(const Transform &transform) {}

    virtual void OnDetachedFromNode(Node *node) {}
    virtual void OnAttachedToNode(Node *node) {}

    virtual void OnDetachedFromScene(ID<Scene> id) {}
    virtual void OnAttachedToScene(ID<Scene> id) {}

    virtual void Serialize(fbom::FBOMObject &out) const
#ifdef HYP_CONTROLLER_SERIALIZATION_METHDOS_PURE_VIRTUAL
        = 0;
#else
    {
#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(false, "Serialize() not implemented for controller!");
#endif
    }
#endif

    virtual fbom::FBOMResult Deserialize(const fbom::FBOMObject &in)
#ifdef HYP_CONTROLLER_SERIALIZATION_METHDOS_PURE_VIRTUAL
        = 0;
#else
    {
#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(false, "Deserialize() not implemented for controller!");
#endif

        return fbom::FBOMResult::FBOM_OK;
    }
#endif

private:
    String m_name;
    Entity *m_owner;
    bool m_receives_update;
};

class ControllerSet
{
    using Map = TypeMap<UniquePtr<Controller>>;

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
    
    void Set(TypeID type_id, UniquePtr<Controller> &&controller)
    {
        AssertThrow(controller != nullptr);

        m_map.Set(type_id, std::move(controller));
    }
    
    template <class T>
    void Set(UniquePtr<T> &&controller)
    {
        AssertThrow(controller != nullptr);

        const auto id = TypeID::ForType<T>();

        m_map.Set(id, std::move(controller));
    }

    Controller *Get(TypeID type_id) const
    {
        const auto it = m_map.Find(type_id);

        if (it == m_map.End()) {
            return nullptr;
        }

        return it->second.Get();
    }

    template <class T>
    T *Get() const
    {
        const auto id = TypeID::ForType<T>();

        return static_cast<T *>(Get(id));
    }

    bool Has(TypeID type_id) const
    {
        return m_map.Find(type_id) != m_map.End();
    }

    template <class T>
    bool Has() const
    {
        const auto id = TypeID::ForType<T>();

        return Has(id);
    }
    
    bool Remove(TypeID type_id)
    {
        const auto it = m_map.Find(type_id);

        if (it == m_map.End()) {
            return false;
        }

        m_map.Erase(it);

        return true;
    }
    
    template <class T>
    bool Remove()
    {
        const auto id = TypeID::ForType<T>();

        return Remove(id);
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