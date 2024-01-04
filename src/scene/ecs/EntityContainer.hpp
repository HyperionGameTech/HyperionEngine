#ifndef HYPERION_V2_ECS_ENTITY_CONTAINER_HPP
#define HYPERION_V2_ECS_ENTITY_CONTAINER_HPP

#include <core/lib/FlatMap.hpp>
#include <core/ID.hpp>
#include <core/Handle.hpp>

namespace hyperion::v2 {

class Entity;

using EntityContainer = FlatMap<ID<Entity>, Handle<Entity>>;

} // namespace hyperion::v2

#endif