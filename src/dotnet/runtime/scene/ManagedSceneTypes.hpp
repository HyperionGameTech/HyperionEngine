/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RUNTIME_DOTNET_MANAGED_SCENE_TYPES_HPP
#define HYPERION_RUNTIME_DOTNET_MANAGED_SCENE_TYPES_HPP

#include <core/ID.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

extern "C" {

struct ManagedEntity
{
    uint32 id;

    ManagedEntity() = default;

    ManagedEntity(ID<Entity> id)
        : id(id.Value())
    {
    }

    operator ID<Entity>() const
        { return ID<Entity> { id }; }
};

static_assert(std::is_trivial_v<ManagedEntity>, "ManagedEntity must be a trivial type to be used in C#");
static_assert(sizeof(ManagedEntity) == 4, "ManagedEntity must be 4 bytes to be used in C#");

} // extern "C"

} // namespace hyperion

#endif