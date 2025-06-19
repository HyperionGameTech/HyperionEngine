/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_SET_BASE_HPP
#define HYPERION_ECS_ENTITY_SET_BASE_HPP

#include <core/ID.hpp>

#include <Types.hpp>

namespace hyperion {

class Entity;

class EntitySetBase;
class ComponentContainerBase;

// @TODO Ensure it is safe across DLLs

class EntitySetBase
{
public:
    EntitySetBase() = default;
    EntitySetBase(const EntitySetBase& other) = delete;
    EntitySetBase& operator=(const EntitySetBase& other) = delete;
    EntitySetBase(EntitySetBase&& other) noexcept = delete;
    EntitySetBase& operator=(EntitySetBase&& other) noexcept = delete;
    virtual ~EntitySetBase() = default;

    virtual SizeType Size() const = 0;

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
    virtual void OnEntityUpdated(ID<Entity> id) = 0;
};

} // namespace hyperion

#endif