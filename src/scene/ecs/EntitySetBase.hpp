/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_SET_BASE_HPP
#define HYPERION_ECS_ENTITY_SET_BASE_HPP

#include <core/memory/UniquePtr.hpp>
#include <core/containers/Array.hpp>
#include <core/IDCreator.hpp>
#include <core/ID.hpp>

#include <scene/ecs/EntityContainer.hpp>

#include <Types.hpp>

#include <atomic>

namespace hyperion {

class Entity;

using EntitySetTypeID = uint;

class EntitySetBase;
class ComponentContainerBase;

struct EntitySetIDGeneratorBase
{
    static inline std::atomic<uint> counter { 0u };
};

template <class ... Components>
struct EntitySetIDGenerator : EntitySetIDGeneratorBase
{
    static EntitySetTypeID GetID()
    {
        static EntitySetTypeID id = ++EntitySetIDGeneratorBase::counter;

        return id;
    }
};

class EntitySetBase
{
public:
    template <class ... Components>
    static EntitySetTypeID GenerateEntitySetTypeID()
    {
        return EntitySetIDGenerator<Components...>::GetID();
    }

    EntitySetBase()                                             = default;
    EntitySetBase(const EntitySetBase &other)                   = delete;
    EntitySetBase &operator=(const EntitySetBase &other)        = delete;
    EntitySetBase(EntitySetBase &&other) noexcept               = delete;
    EntitySetBase &operator=(EntitySetBase &&other) noexcept    = delete;
    virtual ~EntitySetBase()                                    = default;

    /*! \brief Checks if an Entity's components are valid for this EntitySet.
     *
     *  \param entity The Entity to check.
     *
     *  \return True if the Entity's components are valid for this EntitySet, false otherwise.
     */
    virtual bool ValidForEntity(ID<Entity> entity) const = 0;

    /*! \brief Removes the given Entity from this EntitySet.
     *
     *  \param entity The Entity to remove.
     */
    virtual void RemoveEntity(ID<Entity> entity) = 0;

    /*! \brief To be used by the EntityManager
        \note Do not call this function directly. */
    virtual void OnEntityUpdated(ID<Entity> entity) = 0;
};

} // namespace hyperion

#endif