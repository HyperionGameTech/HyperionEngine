/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SCRIPT_COMPONENT_HPP
#define HYPERION_ECS_SCRIPT_COMPONENT_HPP

#include <core/Handle.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>

#include <scripting/Script.hpp>

#include <HashCode.hpp>

namespace hyperion {

enum class ScriptComponentFlags : uint32
{
    NONE                = 0x0,
    INITIALIZED         = 0x1,
    RELOADING           = 0x2,

    BEFORE_INIT_CALLED  = 0x4,
    INIT_CALLED         = 0x8 // the script has already been compiled once, with Init() and BeforeInit() called. don't call them again.
};

HYP_MAKE_ENUM_FLAGS(ScriptComponentFlags);

struct ScriptComponent
{
    ManagedScript                   script { };

    UniquePtr<dotnet::Assembly>     assembly;
    UniquePtr<dotnet::Object>       object;

    EnumFlags<ScriptComponentFlags> flags = ScriptComponentFlags::NONE;

    HYP_NODISCARD HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(script);

        return hash_code;
    }
};

} // namespace hyperion

#endif