/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ECS_SCRIPT_COMPONENT_HPP
#define HYPERION_V2_ECS_SCRIPT_COMPONENT_HPP

#include <core/Handle.hpp>

#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>

#include <HashCode.hpp>

namespace hyperion::v2 {

struct ScriptInfo
{
    String  assembly_name;
    String  class_name;
};

struct ScriptComponent
{
    ScriptInfo                  script_info;

    RC<dotnet::Assembly>        assembly;
    UniquePtr<dotnet::Object>   object;
};

} // namespace hyperion::v2

#endif