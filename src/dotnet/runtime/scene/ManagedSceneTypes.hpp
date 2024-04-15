/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_RUNTIME_DOTNET_MANAGED_SCENE_TYPES_HPP
#define HYPERION_V2_RUNTIME_DOTNET_MANAGED_SCENE_TYPES_HPP

#include <dotnet/runtime/scene/ManagedNode.hpp>

#include <core/ID.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion::v2 {

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
}

} // namespace hyperion::v2

#endif