/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>

namespace hyperion::v2 {

Entity::Entity()
    : BasicObject()
{
}

Entity::~Entity()
{
}

void Entity::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    SetReady(true);
}

bool Entity::IsReady() const
{
    return Base::IsReady();
}

} // namespace hyperion::v2
