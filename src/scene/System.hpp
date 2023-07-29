#ifndef HYPERION_V2_SYSTEM_HPP
#define HYPERION_V2_SYSTEM_HPP

#include <core/Base.hpp>
#include <math/Transform.hpp>
#include <scene/Entity.hpp>
#include <scene/Controller.hpp>
#include <Threads.hpp>
#include "../GameCounter.hpp"

#include <core/Containers.hpp>

#include <Types.hpp>

#include <mutex>

namespace hyperion::v2 {

class Engine;

namespace detail {

class ComponentContainerBase
{
public:
    virtual ~ComponentContainerBase() = default;

    virtual void Update(GameCounter::TickUnit delta) = 0;

protected:
    ComponentContainerBase() = default;

    FlatMap<ID<Entity>, UniquePtr<Controller>> m_entity_to_controller;
};

template <class ControllerType>
class ComponentContainer : public ComponentContainerBase
{
public:
    ComponentContainer();
    ComponentContainer(const ComponentContainer &other) = delete;
    ComponentContainer &operator=(const ComponentContainer &other) = delete;
    virtual ~ComponentContainer() override;

    ControllerType *Add(Entity *entity, UniquePtr<ControllerType> &&controller);
    void Remove(ID<Entity> id);

    ControllerType *Get(ID<Entity> id);

    virtual void Update(GameCounter::TickUnit delta) override;
};

template <class ControllerType>
ComponentContainer<ControllerType>::ComponentContainer()
    : ComponentContainerBase()
{
}

template <class ControllerType>
ComponentContainer<ControllerType>::~ComponentContainer()
{
    auto copy = ComponentContainerBase::m_entity_to_controller;

    for (auto &it : copy) {
        Remove(it.first);
    }
}

template <class ControllerType>
ControllerType *ComponentContainer<ControllerType>::Add(Entity *entity, UniquePtr<ControllerType> &&controller)
{
    AssertThrow(entity != nullptr);
    AssertThrow(controller != nullptr);
    AssertThrow(controller->m_owner == nullptr);

    controller->m_owner = entity;
    controller->OnAdded();

    for (Node *node : entity->m_nodes) {
        controller->OnAttachedToNode(node);
    }

    for (const ID<Scene> &id : entity->GetScenes()) {
        controller->OnAttachedToScene(id);
    }

    controller->OnTransformUpdate(entity->GetTransform());

    auto it = ComponentContainerBase::m_entity_to_controller.Find(entity->GetID());

    if (it != ComponentContainerBase::m_entity_to_controller.End()) {
        AssertThrow(it->second != nullptr);

        ControllerType *found_controller = static_cast<ControllerType *>(it->second.Get());

        if (found_controller->m_owner->m_node != nullptr) {
            controller->OnDetachedFromNode(found_controller->m_owner->m_node);
        }

        if (found_controller->m_owner->m_scene != nullptr) {
            controller->OnDetachedFromScene(found_controller->m_owner->m_scene);
        }

        found_controller->OnRemoved();

        it->second = std::move(controller);

        return static_cast<ControllerType *>(it->second.Get());
    } else {
        return static_cast<ControllerType *>(ComponentContainerBase::m_entity_to_controller.Set(entity->GetID(), std::move(controller)).first.Get());
    }
}

template <class ControllerType>
void ComponentContainer<ControllerType>::Remove(ID<Entity> id)
{
    auto it = ComponentContainerBase::m_entity_to_controller.Find(id);

    if (it == ComponentContainerBase::m_entity_to_controller.End()) {
        return;
    }

    ControllerType *controller = static_cast<ControllerType *>(it->second.Get());

    if (controller->m_owner->m_node != nullptr) {
        controller->OnDetachedFromNode(controller->m_owner->m_node);
    }

    if (controller->m_owner->m_scene != nullptr) {
        controller->OnDetachedFromScene(controller->m_owner->m_scene);
    }

    controller->OnRemoved();
}

template <class ControllerType>
ControllerType *ComponentContainer<ControllerType>::Get(ID<Entity> id)
{
    auto it = ComponentContainerBase::m_entity_to_controller.Find(id);

    if (it == ComponentContainerBase::m_entity_to_controller.End()) {
        return nullptr;
    }

    return static_cast<ControllerType *>(it->second.Get());
}

template <class ControllerType>
void ComponentContainer<ControllerType>::Update(GameCounter::TickUnit delta)
{
    for (auto &it : ComponentContainerBase::m_entity_to_controller) {
        static_cast<ControllerType *>(it->second.Get())->ControllerType::OnUpdate(delta);
    }
}

} // namespace detail

struct RegisteredController
{
    using ControllerCreateFunction = std::add_pointer_t<UniquePtr<Controller>(void)>;

    TypeID type_id;
    ANSIString name;
    ControllerCreateFunction create_fn;
};

class ComponentRegistry
{
public:
    template <class ControllerType>
    void Register()
    {
        static_assert(std::is_base_of_v<Controller, ControllerType>, "ControllerType must be a controller!");

        const ANSIString controller_name(ControllerType::controller_name);

        const auto it = m_registered_controllers.Find(controller_name);

        AssertThrowMsg(
            it == m_registered_controllers.End(),
            "Controller %s is already registered!",
            controller_name.Data()
        );

        m_registered_controllers[controller_name] = RegisteredController {
            TypeID::ForType<ControllerType>(),
            controller_name,
            []() -> UniquePtr<Controller> {
                return UniquePtr<ControllerType>::Construct();
            }
        };

        DebugLog(
            LogType::Debug,
            "Registered controller %s\n",
            controller_name.Data()
        );
    }

    bool IsRegistered(const ANSIString &controller_name) const
    {
        return m_registered_controllers.Contains(controller_name);
    }

    UniquePtr<Controller> CreateByName(const ANSIString &controller_name)
    {
        const auto it = m_registered_controllers.Find(controller_name);

        AssertThrowMsg(
            it != m_registered_controllers.End(),
            "Controller %s is not registered!",
            controller_name.Data()
        );

        return it->second.create_fn();
    }

    TypeID GetControllerTypeID(const ANSIString &controller_name) const
    {
        const auto it = m_registered_controllers.Find(controller_name);

        if (it == m_registered_controllers.End()) {
            return TypeID::ForType<void>();
        }

        return it->second.type_id;
    }

    template <class ControllerType>
    void Add(Entity *entity, UniquePtr<ControllerType> &&controller)
    {
        Threads::AssertOnThread(THREAD_GAME);

        GetComponentContainer<ControllerType>()->Add(entity, std::move(controller));
    }

    template <class ControllerType>
    void Remove(ID<Entity> id)
    {
        Threads::AssertOnThread(THREAD_GAME);

        GetComponentContainer<ControllerType>()->Remove(id);
    }

    void Update(GameCounter::TickUnit delta)
    {
        Threads::AssertOnThread(THREAD_GAME);

        for (auto &it : m_systems) {
            it.second->Update(delta);
        }
    }

private:
    template <class ControllerType>
    detail::ComponentContainer<ControllerType> *GetComponentContainer()
    {
        if (!m_systems.Contains<ControllerType>()) {
            m_systems.Set<ControllerType>(UniquePtr<detail::ComponentContainer<ControllerType>>::Construct());
        }

        return static_cast<detail::ComponentContainer<ControllerType> *>(m_systems.At<ControllerType>().Get());
    }

    FlatMap<ANSIString, RegisteredController> m_registered_controllers;
    TypeMap<UniquePtr<detail::ComponentContainerBase>> m_systems;
};

} // namespace hyperion::v2

#endif