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

class ASTType;

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
    RC<ASTType>                                 cxx_type;
    String                                      source;

    bool HasAttribute(UTF8StringView key) const
    {
        auto it = attributes.FindIf([key_lower = String(key).ToLower()](const auto &item)
        {
            return item.first.ToLower() == key_lower;
        });

        return it != attributes.End();
    }

    const HypClassAttributeValue &GetAttribute(UTF8StringView key) const
    {
        auto it = attributes.FindIf([key_lower = String(key).ToLower()](const auto &item)
        {
            return item.first.ToLower() == key_lower;
        });

        return it != attributes.End()
            ? it->second
            : HypClassAttributeValue::empty;
    }

    bool AddAttribute(const String &key, const HypClassAttributeValue &value)
    {
        if (HasAttribute(key)) {
            return false;
        }

        attributes.PushBack({ key, value });

        return true;
    }
};

struct HypClassDefinition
{
    HypClassDefinitionType                      type;
    String                                      name;
    Array<Pair<String, HypClassAttributeValue>> attributes;
    Array<String>                               base_class_names;
    Array<HypMemberDefinition>                  members;
    String                                      source;

    HYP_FORCE_INLINE bool HasAttribute(UTF8StringView key) const
    {
        auto it = attributes.FindIf([key_lower = String(key).ToLower()](const auto &item)
        {
            return item.first.ToLower() == key_lower;
        });

        return it != attributes.End();
    }

    HYP_FORCE_INLINE const HypClassAttributeValue &GetAttribute(UTF8StringView key) const
    {
        auto it = attributes.FindIf([key_lower = String(key).ToLower()](const auto &item)
        {
            return item.first.ToLower() == key_lower;
        });

        return it != attributes.End()
            ? it->second
            : HypClassAttributeValue::empty;
    }

    HYP_FORCE_INLINE bool HasScriptableMethods() const
    {
        for (const HypMemberDefinition &member : members) {
            if (member.HasAttribute("scriptable")) {
                return true;
            }
        }

        return false;
    }
};

} // namespace buildtool
} // namespace hyperion

#endif
