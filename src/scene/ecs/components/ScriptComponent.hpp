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

HYP_STRUCT()
struct ScriptComponent
{
    HYP_FIELD(SerializeAs=Script)
    ManagedScript                   script = { };

    HYP_FIELD()
    UniquePtr<dotnet::Assembly>     assembly;

    HYP_FIELD()
    UniquePtr<dotnet::Object>       object;

    HYP_FIELD()
    EnumFlags<ScriptComponentFlags> flags = ScriptComponentFlags::NONE;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(script);

        return hash_code;
    }
};

} // namespace hyperion

#endif