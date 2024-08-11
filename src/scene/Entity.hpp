/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_HPP
#define HYPERION_ENTITY_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

namespace hyperion {

/*! \brief Do not use this class directly. Use ID<Entity> instead. */
class Entity : public BasicObject<Entity>
{
    HYP_OBJECT_BODY(Entity);

public:
    Entity() = default;
};

} // namespace hyperion

#endif
