/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

static int TestHelperMethod(const Entity *entity)
{
    return 1234;
}

// @TODO Add helpers so we can get components from an entity
HYP_DEFINE_CLASS(
    Entity,
    HypProperty(NAME("ID"), &Entity::GetID),
    HypProperty(NAME("Components"), &TestHelperMethod)
);

} // namespace hyperion