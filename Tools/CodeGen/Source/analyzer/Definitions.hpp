/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#ifndef HYPERION_CODEGEN_DEFINITIONs_HPP
#define HYPERION_CODEGEN_DEFINITIONs_HPP

#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>

#include <Core/memory/UniquePtr.hpp>
#include <Core/memory/RefCountedPtr.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/StringView.hpp>

#include <Core/reflection/Member.hpp>
#include <Core/reflection/ClassAttribute.hpp>

#include <Core/logging/Logger.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {
namespace CodeGen {

HYP_DECLARE_LOG_CHANNEL(Tool);

struct ASTType;
struct ASTMemberDecl;
class Analyzer;
class Module;

enum class ClassDefinitionType : int
{
    None = 0,
    Class,
    Struct,
    Enum,
    Max
};

struct MemberDef
{
    MemberType type;
    String name;
    String friendlyName;
    Array<Pair<String, ClassAttributeValue>> attributes;
    RC<ASTType> cxxType;
    RC<ASTMemberDecl> cxxDecl;
    String source;

    bool HasAttribute(const String& key) const
    {
        const String keyLower = key.ToLower();

        auto it = attributes.FindIf([&keyLower](const auto& item)
            {
                return item.first.ToLower() == keyLower;
            });

        return it != attributes.End();
    }

    const ClassAttributeValue& GetAttribute(const String& key) const
    {
        const String keyLower = key.ToLower();

        auto it = attributes.FindIf([&keyLower](const auto& item)
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
    Array<MemberDef> members;
    Array<String> namespaceParts;
    String condition; // preprocessor condition for this class to be defined
    String source;
    Module* declModule = nullptr;

    // cached C++ parsing metadata, used for building forward declarations
    bool isCXXClass : 1;     //!< original type is class
    bool isCXXStruct : 1;    //!< original type is struct
    bool isCXXEnum : 1;      //!< original type is enum
    bool isCXXEnumClass : 1; //!< original type is enum class

    ClassDefinition()
        : type(ClassDefinitionType::None),
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

    bool HasAttribute(const String& key) const
    {
        const String keyLower = key.ToLower();
        
        auto it = attributes.FindIf([&keyLower](const auto& item)
            {
                return item.first.ToLower() == keyLower;
            });

        return it != attributes.End();
    }

    const ClassAttributeValue& GetAttribute(const String& key) const
    {
        const String keyLower = key.ToLower();

        auto it = attributes.FindIf([&keyLower](const auto& item)
            {
                return item.first.ToLower() == keyLower;
            });

        return it != attributes.End()
            ? it->second
            : ClassAttributeValue::empty;
    }

    bool HasScriptableMethods() const
    {
        static const String s_scriptableStr = "scriptable";
        
        for (const MemberDef& member : members)
        {
            if (member.HasAttribute(s_scriptableStr))
            {
                return true;
            }
        }

        return false;
    }
};

} // namespace CodeGen
} // namespace Hyperion

#endif
