#ifndef HYPERION_V2_ENTITY_COMPONENT_MANAGER_HPP
#define HYPERION_V2_ENTITY_COMPONENT_MANAGER_HPP

#include <core/Base.hpp>
#include <core/Name.hpp>
#include <core/lib/Mutex.hpp>
#include <core/lib/AtomicVar.hpp>
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
    using Iterator      = typename FlatMap<ID<Entity>, UniquePtr<Controller>>::Iterator;
    using ConstIterator = typename FlatMap<ID<Entity>, UniquePtr<Controller>>::ConstIterator;

    virtual ~ComponentContainerBase() = default;

    Controller *Add(Entity *entity, UniquePtr<Controller> &&controller);
    Controller *Get(ID<Entity> id);
    Bool Remove(ID<Entity> id);

    virtual void Update(GameCounter::TickUnit delta) = 0;

    typename FlatMap<ID<Entity>, UniquePtr<Controller>>::Iterator Find(ID<Entity> id)
        { return m_entity_to_controller.Find(id); }

    HYP_DEF_STL_BEGIN_END(
        ComponentContainerBase::m_entity_to_controller.Begin(),
        ComponentContainerBase::m_entity_to_controller.End()
    )

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
    const FlatSet<ID<Entity>> keys = m_entity_to_controller.Keys();

    for (ID<Entity> key : keys) {
        Remove(key);
    }
}

template <class ControllerType>
void ComponentContainer<ControllerType>::Update(GameCounter::TickUnit delta)
{
    for (auto &it : m_entity_to_controller) {
        if (!it.second.Get()->ReceivesUpdate()) {
            continue; // @TODO: Make this be for the 
        }

        static_cast<ControllerType *>(it.second.Get())->ControllerType::OnUpdate(delta);
    }
}

} // namespace detail

struct RegisteredController
{
    using ControllerCreateFunction = UniquePtr<Controller>(*)(void);
    using ComponentContainerCreateFunction = UniquePtr<detail::ComponentContainerBase>(*)(void);

    TypeID                              type_id;
    Name                                name;
    ControllerCreateFunction            controller_create_fn;
    ComponentContainerCreateFunction    component_container_create_fn;

    UniquePtr<Controller> CreateController() const
        { return controller_create_fn(); }

    UniquePtr<detail::ComponentContainerBase> CreateComponentContainer() const
        { return component_container_create_fn(); }
};

struct EntityComponentIterator
{
private:
    using ComponentContainerIterator    = typename TypeMap<UniquePtr<detail::ComponentContainerBase>>::Iterator;
    using ComponentIterator             = typename detail::ComponentContainerBase::Iterator;

public:
    

    EntityComponentIterator(ID<Entity> entity_id, ComponentContainerIterator containers_begin, ComponentContainerIterator containers_end)
        : entity_id(entity_id),
          containers_it(containers_begin),
          containers_end(containers_end),
          component_it(nullptr)
    {
        while (containers_it != containers_end && (component_it = containers_it->second->Find(entity_id)) == containers_it->second->End()) {
            ++containers_it;
        }

        if (containers_it == containers_end) {
            component_it = nullptr;
        }
    }

    EntityComponentIterator(const EntityComponentIterator &other)
        : entity_id(other.entity_id),
          containers_it(other.containers_it),
          containers_end(other.containers_end),
          component_it(other.component_it)
    {
    }

    EntityComponentIterator &operator=(const EntityComponentIterator &other)
    {
        entity_id = other.entity_id;
        containers_it = other.containers_it;
        containers_end = other.containers_end;
        component_it = other.component_it;

        return *this;
    }

    EntityComponentIterator(EntityComponentIterator &&other) noexcept
        : entity_id(other.entity_id),
          containers_it(other.containers_it),
          containers_end(other.containers_end),
          component_it(other.component_it)
    {
        other.containers_it = other.containers_end;
        other.component_it = nullptr;
    }

    EntityComponentIterator &operator=(EntityComponentIterator &&other) noexcept
    {
        entity_id = other.entity_id;
        containers_it = other.containers_it;
        containers_end = other.containers_end;
        component_it = other.component_it;

        other.containers_it = other.containers_end;
        other.component_it = nullptr;

        return *this;
    }

    Bool operator==(const EntityComponentIterator &other) const
    {
        return entity_id == other.entity_id
            && containers_it == other.containers_it
            && containers_end == other.containers_end
            && component_it == other.component_it;
    }

    Bool operator!=(const EntityComponentIterator &other) const
        { return !(*this == other); }

    EntityComponentIterator &operator++()
    {
        do {
            ++containers_it;
        } while (containers_it != containers_end && (component_it = containers_it->second->Find(entity_id)) == containers_it->second->End());

        if (containers_it == containers_end) {
            component_it = nullptr;
        }

        return *this;
    }

    EntityComponentIterator operator++(int)
    {
        EntityComponentIterator copy = *this;

        ++(*this);

        return copy;
    }

    Controller *operator->() const
        { return component_it->second.Get(); }

    Controller &operator*()
        { return *component_it->second; }

    const Controller &operator*() const
        { return *component_it->second; }

private:
    ID<Entity>                  entity_id;
    ComponentContainerIterator  containers_it;
    ComponentContainerIterator  containers_end;
    ComponentIterator           component_it;
};

class EntityComponentManager
{
public:
    template <class ControllerType>
    void Register()
    {
        static_assert(std::is_base_of_v<Controller, ControllerType>, "ControllerType must be a controller!");

        const Name controller_name = CreateNameFromDynamicString(ControllerType::controller_name);

        Optional<RegisteredController> registered_controller = GetRegisteredController(controller_name);

        AssertThrowMsg(
            registered_controller.Empty(),
            "Controller %s is already registered!",
            controller_name.LookupString().Data()
        );

        m_registered_controllers[controller_name] = RegisteredController {
            TypeID::ForType<ControllerType>(),
            controller_name,
            []() -> UniquePtr<Controller>
            {
                return UniquePtr<ControllerType>::Construct();
            },
            []() -> UniquePtr<detail::ComponentContainerBase>
            {
                return UniquePtr<detail::ComponentContainer<ControllerType>>::Construct();
            }
        };

        DebugLog(
            LogType::Debug,
            "Registered controller %s\n",
            controller_name.LookupString().Data()
        );
    }

    Bool IsRegistered(Name controller_name) const
        { return GetRegisteredController(controller_name).HasValue(); }

    Bool IsRegistered(TypeID controller_type_id) const
        { return GetRegisteredController(controller_type_id).HasValue(); }

    TypeID GetControllerTypeID(Name controller_name) const
    {
        Optional<RegisteredController> registered_controller = GetRegisteredController(controller_name);

        AssertThrowMsg(
            registered_controller.HasValue(),
            "Controller %s is not registered!",
            controller_name.LookupString().Data()
        );

        return registered_controller.Get().type_id;
    }

    UniquePtr<Controller> CreateByName(Name controller_name)
    {
        Optional<RegisteredController> registered_controller = GetRegisteredController(controller_name);

        AssertThrowMsg(
            registered_controller.HasValue(),
            "Controller %s is not registered!",
            controller_name.LookupString().Data()
        );

        return registered_controller.Get().CreateController();
    }

    template <class ControllerType>
    UniquePtr<ControllerType> Create()
    {
        static_assert(std::is_base_of_v<Controller, ControllerType>, "ControllerType must be a controller!");

        // Ensure that the controller is registered
        const Name controller_name = CreateNameFromDynamicString(ControllerType::controller_name);

        Optional<RegisteredController> registered_controller = GetRegisteredController(controller_name);

        AssertThrowMsg(
            registered_controller.HasValue(),
            "Controller %s is not registered!",
            controller_name.LookupString().Data()
        );

        return registered_controller.Get().CreateController().Cast<ControllerType>();
    }

    /*! \brief Add a component to the given entity.
     *
     *  \param entity The entity to add the controller to.
     *  \param component_type_id The type ID of the controller to add.
     *  \param component The component to add.
     */
    Controller *Add(Handle<Entity> entity, TypeID component_type_id, UniquePtr<Controller> &&component)
    {
        AssertThrow(entity.IsValid());
        AssertThrow(component != nullptr);

        Mutex::Guard guard(m_mutex);

        auto pending_removal_it = m_components_pending_removal.Find(component_type_id);

        if (pending_removal_it != m_components_pending_removal.End()) {
            pending_removal_it->second.Erase(entity.GetID());
        }

        auto pending_addition_it = m_components_pending_addition.Find(component_type_id);

        if (pending_addition_it == m_components_pending_addition.End()) {
            auto insert_result = m_components_pending_addition.Set(component_type_id, { });

            pending_addition_it = insert_result.first;
        }

        Controller *ptr = component.Get();

        pending_addition_it->second.PushBack({ std::move(entity), std::move(component) });

        m_has_pending_components.Set(true, MemoryOrder::RELAXED);

        return ptr;
    }

    /*! \brief Add a component to the given entity.
     *
     *  \param entity The entity to add the component to.
     *  \param component The component to add.
     */
    template <class ComponentType>
    ComponentType *Add(Handle<Entity> entity, UniquePtr<ComponentType> &&component)
    {
        return static_cast<ComponentType *>(Add(std::move(entity), TypeID::ForType<ComponentType>(), std::move(component)));
    }

    /*! \brief Removes all controllers of the given entity.
     *
     *  \param entity_id The entity ID.
     */
    void Remove(ID<Entity> id)
    {
        Mutex::Guard guard(m_mutex);

        for (auto &it : m_components_pending_addition) {
            auto &array = it.second;

            for (auto array_it = array.Begin(); array_it != array.End();) {
                if (array_it->first.GetID() == id) {
                    array_it = array.Erase(array_it);
                } else {
                    ++array_it;
                }
            }
        }

        for (auto &it : m_components_pending_removal) {
            it.second.Insert(id);
        }

        m_has_pending_components.Set(true, MemoryOrder::RELAXED);
    }

    /*! \brief Removes the first controller of the given entity for the given type.
     *
     *  \param entity_id The entity ID.
     */
    void Remove(TypeID controller_type_id, ID<Entity> id)
    {
        Mutex::Guard guard(m_mutex);

        auto pending_addition_it = m_components_pending_addition.Find(controller_type_id);

        if (pending_addition_it != m_components_pending_addition.End()) {
            auto &array = pending_addition_it->second;

            for (auto array_it = array.Begin(); array_it != array.End();) {
                if (array_it->first.GetID() == id) {
                    array_it = array.Erase(array_it);
                } else {
                    ++array_it;
                }
            }
        }

        auto pending_removal_it = m_components_pending_removal.Find(controller_type_id);

        if (pending_removal_it == m_components_pending_removal.End()) {
            auto insert_result = m_components_pending_removal.Set(controller_type_id, { });

            pending_removal_it = insert_result.first;
        }

        pending_removal_it->second.Insert(id);

        m_has_pending_components.Set(true, MemoryOrder::RELAXED);
    }

    /*! \brief Removes the first controller of the given entity for the given type.
     *
     *  \param entity_id The entity ID.
     */
    template <class ControllerType>
    void Remove(ID<Entity> id)
        { Remove(TypeID::ForType<ControllerType>(), id); }

    /*! \brief Returns the first controller of the given entity for the given type.
     * Note: Only use this on the game thread!
     *
     *  \param entity_id The entity ID.
     */
    template <class ControllerType>
    ControllerType *Get(ID<Entity> id)
    {
        Threads::AssertOnThread(THREAD_GAME);

        return static_cast<ControllerType *>(GetComponentContainer<ControllerType>()->Get(id));
    }

    /*! \brief Returns true if the given entity has a controller of the given type.
     * Note: Only use this on the game thread!
     *
     *  \param entity_id The entity ID.
     */
    template <class ControllerType>
    Bool Has(ID<Entity> id) const
    {
        Threads::AssertOnThread(THREAD_GAME);

        return GetComponentContainer<ControllerType>()->Get(id) != nullptr;
    }

    void Update(GameCounter::TickUnit delta)
    {
        Threads::AssertOnThread(THREAD_GAME);

        AddPendingComponents();

        for (auto &it : m_components) {
            it.second->Update(delta);
        }
    }

    /*! \brief Returns an iterator to the first controller of the given entity.
     *  Note: Only use this on the game thread!
     *
     *  \param entity_id The entity ID.
     *
     *  \return An iterator to the first controller of the given entity.
     */
    EntityComponentIterator Begin(ID<Entity> entity_id)
        { return EntityComponentIterator(entity_id, m_components.Begin(), m_components.End()); }

    /*! \brief Returns an iterator to the end of the controllers of the given entity.
     *  Note: Only use this on the game thread!
     *
     *  \param entity_id The entity ID.
     *
     *  \return An iterator to the end of the controllers of the given entity.
     */
    EntityComponentIterator End(ID<Entity> entity_id)
        { return EntityComponentIterator(entity_id, m_components.End(), m_components.End()); }

    TypeMap<UniquePtr<detail::ComponentContainerBase>> &GetComponents()
        { return m_components; }

    const TypeMap<UniquePtr<detail::ComponentContainerBase>> &GetComponents() const
        { return m_components; }

private:
    Optional<RegisteredController> GetRegisteredController(Name controller_name) const
    {
        const auto it = m_registered_controllers.Find(controller_name);

        if (it == m_registered_controllers.End()) {
            return { };
        }

        return it->second;
    }

    Optional<RegisteredController> GetRegisteredController(TypeID controller_type_id) const
    {
        for (auto &it : m_registered_controllers) {
            if (it.second.type_id == controller_type_id) {
                return it.second;
            }
        }

        return { };
    }

    detail::ComponentContainerBase *GetComponentContainer(TypeID component_type_id)
    {
        // Component must be registered
        Optional<RegisteredController> registered_controller = GetRegisteredController(component_type_id);

        AssertThrowMsg(
            registered_controller.HasValue(),
            "Component with type ID %u is not registered!",
            component_type_id.Value()
        );

        if (!m_components.Contains(component_type_id)) {
            m_components.Set(component_type_id, registered_controller.Get().CreateComponentContainer());
        }

        return m_components.Get(component_type_id).Get();
    }

    template <class ControllerType>
    detail::ComponentContainer<ControllerType> *GetComponentContainer()
    {
        return static_cast<detail::ComponentContainer<ControllerType> *>(GetComponentContainer(TypeID::ForType<ControllerType>()));
    }

    void AddPendingComponents()
    {
        if (!m_has_pending_components.Get(MemoryOrder::RELAXED)) {
            return;
        }

        Mutex::Guard guard(m_mutex);

        for (auto &it : m_components_pending_addition) {
            auto *container = GetComponentContainer(it.first);

            for (auto &it2 : it.second) {
                AssertThrow(it2.first.IsValid());

                container->Add(it2.first.Get(), std::move(it2.second));
            }
        }

        m_components_pending_addition.Clear();

        for (auto &it : m_components_pending_removal) {
            auto *container = GetComponentContainer(it.first);

            for (ID<Entity> id : it.second) {
                container->Remove(id);
            }
        }

        m_components_pending_removal.Clear();

        m_has_pending_components.Set(false, MemoryOrder::RELAXED);
    }

    FlatMap<Name, RegisteredController>                         m_registered_controllers;
    TypeMap<UniquePtr<detail::ComponentContainerBase>>          m_components;
    TypeMap<Array<Pair<Handle<Entity>, UniquePtr<Controller>>>> m_components_pending_addition;
    TypeMap<FlatSet<ID<Entity>>>                                m_components_pending_removal;
    Mutex                                                       m_mutex;
    AtomicVar<Bool>                                             m_has_pending_components;
};

} // namespace hyperion::v2

#endif