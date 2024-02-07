#ifndef HYPERION_V2_ECS_ENTITY_MANAGER_COMMAND_HPP
#define HYPERION_V2_ECS_ENTITY_MANAGER_COMMAND_HPP

#include <core/lib/TypeID.hpp>
#include <core/ID.hpp>
#include <core/lib/Proc.hpp>
#include <GameCounter.hpp>

namespace hyperion::v2 {

class Entity;
class EntityManager;

using ComponentID = UInt;

using EntityManagerCommandProc = Proc<void, EntityManager &/* mgr*/, GameCounter::TickUnit /* delta */>;

class EntityManagerCommand
{
public:
    EntityManagerCommand(EntityManagerCommandProc &&proc)
        : m_proc(std::move(proc))
    {
    }

    EntityManagerCommand(const EntityManagerCommand &other)                 = delete;
    EntityManagerCommand &operator=(const EntityManagerCommand &other)      = delete;
    EntityManagerCommand(EntityManagerCommand &&other) noexcept             = default;
    EntityManagerCommand &operator=(EntityManagerCommand &&other) noexcept  = default;

    void Execute(EntityManager &manager);

private:
    EntityManagerCommandProc    m_proc;
};

// class AddComponentCommand : public EntityManagerCommand
// {
// public:
//     AddComponentCommand(ID<Entity> entity_id, TypeID component_type_id, UniquePtr<void> &&component)
//         : m_entity_id(entity_id),
//           m_component_type_id(component_type_id),
//           m_component(std::move(component))
//     {
//     }

//     virtual ~AddComponentCommand() override = default;

//     virtual void Execute(EntityManager &manager) override;

// private:
//     ID<Entity>      m_entity_id;
//     TypeID          m_component_type_id;
//     UniquePtr<void> m_component;
// };

// class RemoveComponentCommand : public EntityManagerCommand
// {
// public:
//     RemoveComponentCommand(ID<Entity> entity_id, TypeID component_type_id, ComponentID component_id)
//         : m_entity_id(entity_id),
//           m_component_type_id(component_type_id),
//           m_component_id(component_id)
//     {   
//     }

//     virtual ~RemoveComponentCommand() override = default;

//     virtual void Execute(EntityManager &manager) override;

// private:
//     ID<Entity>  m_entity_id;
//     TypeID      m_component_type_id;
//     ComponentID m_component_id;
// };

// class AddEntityCommand : public EntityManagerCommand
// {
// public:
//     AddEntityCommand()
//     {
//     }

//     virtual ~AddEntityCommand() override = default;

//     virtual void Execute(EntityManager &manager) override;
// };

// class RemoveEntityCommand : public EntityManagerCommand
// {
// public:
//     RemoveEntityCommand(ID<Entity> entity_id)
//         : m_entity_id(entity_id)
//     {
//     }

//     virtual ~RemoveEntityCommand() override = default;

//     virtual void Execute(EntityManager &manager) override;

// private:
//     ID<Entity> m_entity_id;
// };

} // namespace hyperion::v2

#endif