/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_DEFINITIONs_HPP
#define HYPERION_BUILDTOOL_DEFINITIONs_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/object/HypMemberFwd.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace buildtool {

struct TypeDefinition
{
    String                                          name;
    Array<Pair<String, UniquePtr<TypeDefinition>>>  template_args;
};

enum class HypClassDefinitionType
{
    CLASS,
    STRUCT,
    ENUM
};

struct HypMemberDefinition
{
    HypMemberType                       type;
    String                              name;
    TypeDefinition                      return_type;
    Array<Pair<String, TypeDefinition>> parameters;
};

struct HypClassDefinition
{
    HypClassDefinitionType      type;
    String                      name;
    Array<String>               attributes;
    Array<HypMemberDefinition>  members;
};

} // namespace buildtool
} // namespace hyperion

#endif
