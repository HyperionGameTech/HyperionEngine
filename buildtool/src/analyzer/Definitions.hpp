/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_DEFINITIONs_HPP
#define HYPERION_BUILDTOOL_DEFINITIONs_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/StringView.hpp>

#include <core/reflection/HypMemberFwd.hpp>
#include <core/reflection/ClassAttribute.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace buildtool {

struct ASTType;
struct ASTMemberDecl;
class Analyzer;
class Module;

enum class ClassDefinitionType : int
{
    NONE = 0,
    CLASS,
    STRUCT,
    ENUM,

    MAX
};

struct HypMemberDefinition
{
    HypMemberType type;
    String name;
    String friendlyName;
    Array<Pair<String, ClassAttributeValue>> attributes;
    RC<ASTType> cxxType;
    RC<ASTMemberDecl> cxxDecl;
    String source;

    bool HasAttribute(UTF8StringView key) const
    {
        auto it = attributes.FindIf([keyLower = String(key).ToLower()](const auto& item)
            {
                return item.first.ToLower() == keyLower;
            });

        return it != attributes.End();
    }

    const ClassAttributeValue& GetAttribute(UTF8StringView key) const
    {
        auto it = attributes.FindIf([keyLower = String(key).ToLower()](const auto& item)
            {
                return item.first.ToLower() == keyLower;
            });

        return it != attributes.End()
            ? it->second
            : ClassAttributeValue::empty;
    }

    bool AddAttribute(const String& key, const ClassAttributeValue& value)
    {
        if (HasAttribute(key))
        {
            return false;
        }

        attributes.PushBack({ key, value });

        return true;
    }
};

struct ClassDefinition
{
    ClassDefinitionType type;
    String name;
    int staticIndex = -1;
    uint32 numDescendants = 0;
    Array<Pair<String, ClassAttributeValue>> attributes;
    Array<String> baseClassNames;
    Array<HypMemberDefinition> members;
    String source;
    Module* declModule = nullptr;

    // cached C++ parsing metadata, used for building forward declarations
    bool isCXXClass : 1;     //<! original type is class
    bool isCXXStruct : 1;    //<! original type is struct
    bool isCXXEnum : 1;      //<! original type is enum
    bool isCXXEnumClass : 1; //<! original type is enum class

    ClassDefinition()
        : type(ClassDefinitionType::NONE),
          isCXXClass(false),
          isCXXStruct(false),
          isCXXEnum(false),
          isCXXEnumClass(false)
    {
    }

    ClassDefinition(const ClassDefinition& other) = default;
    ClassDefinition& operator=(const ClassDefinition& other) = default;

    ClassDefinition(ClassDefinition&& other) noexcept = default;
    ClassDefinition& operator=(ClassDefinition&& other) noexcept = default;

    ~ClassDefinition() = default;

    HYP_FORCE_INLINE bool HasAttribute(UTF8StringView key) const
    {
        auto it = attributes.FindIf([keyLower = String(key).ToLower()](const auto& item)
            {
                return item.first.ToLower() == keyLower;
            });

        return it != attributes.End();
    }

    HYP_FORCE_INLINE const ClassAttributeValue& GetAttribute(UTF8StringView key) const
    {
        auto it = attributes.FindIf([keyLower = String(key).ToLower()](const auto& item)
            {
                return item.first.ToLower() == keyLower;
            });

        return it != attributes.End()
            ? it->second
            : ClassAttributeValue::empty;
    }

    HYP_FORCE_INLINE bool HasScriptableMethods() const
    {
        for (const HypMemberDefinition& member : members)
        {
            if (member.HasAttribute("scriptable"))
            {
                return true;
            }
        }

        return false;
    }
};

} // namespace buildtool
} // namespace hyperion

#endif
