/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_HPP
#define HYPERION_ENTITY_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

namespace hyperion {

// @TODO Change Entity class to EntityInstance and define a typedef for Entity to Handle<EntityInstance>, and WeakEntity to WeakHandle<EntityInstance>
// Start using Entity and WeakEntity rather than ID<Entity> everywhere so the IDs are properly ref counted.
// Will require a lot of manual refactoring to figure out where we need the reference tracked and we'll need to use Entity or WeakEntity.
// some places (like function params where the entity is never stored) can just use ID<EntityInstance>

/*! \brief Do not use this class directly. Use ID<Entity> instead. */
HYP_CLASS()
class Entity : public BasicObject<Entity>
{
    HYP_OBJECT_BODY(Entity);

    HYP_PROPERTY(ID, &Entity::GetID, { { "serialize", false } });

public:
    HYP_API Entity();
    HYP_API ~Entity();
};

} // namespace hyperion

#endif
