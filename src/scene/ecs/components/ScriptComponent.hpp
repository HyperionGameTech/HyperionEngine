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
    NONE        = 0x0,
    INITIALIZED = 0x1,
    RELOADING   = 0x2
};

HYP_MAKE_ENUM_FLAGS(ScriptComponentFlags);

struct ScriptComponent
{
    ManagedScript                   script { };

    RC<dotnet::Assembly>            assembly;
    UniquePtr<dotnet::Object>       object;

    EnumFlags<ScriptComponentFlags> flags = ScriptComponentFlags::NONE;
};

} // namespace hyperion

#endif