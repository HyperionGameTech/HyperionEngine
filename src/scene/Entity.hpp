/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_HPP
#define HYPERION_ENTITY_HPP

#include <core/Base.hpp>

namespace hyperion {

/*! \brief Do not use this class directly. Use ID<Entity> instead. */
class Entity : public BasicObject<Entity>
{
public:
    Entity() = default;
};

} // namespace hyperion

#endif
