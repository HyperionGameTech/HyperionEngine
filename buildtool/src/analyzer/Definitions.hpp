/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_DEFINITIONs_HPP
#define HYPERION_BUILDTOOL_DEFINITIONs_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/object/HypMemberFwd.hpp>
#include <core/object/HypClassAttribute.hpp>

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
    NONE    = 0,
    CLASS,
    STRUCT,
    ENUM
};

struct HypMemberDefinition
{
    HypMemberType                               type;
    String                                      name;
    Array<Pair<String, HypClassAttributeValue>> attributes;
    TypeDefinition                              return_type;
    Array<Pair<String, TypeDefinition>>         parameters;
    String                                      source;
};

struct HypClassDefinition
{
    HypClassDefinitionType                      type;
    String                                      name;
    Array<Pair<String, HypClassAttributeValue>> attributes;
    Array<String>                               base_class_names;
    Array<HypMemberDefinition>                  members;
    String                                      source;
};

} // namespace buildtool
} // namespace hyperion

#endif
