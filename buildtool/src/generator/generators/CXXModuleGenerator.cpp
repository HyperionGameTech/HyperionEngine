/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <generator/generators/CXXModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/containers/HashSet.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>

#include <core/utilities/StringUtil.hpp>

namespace hyperion {
namespace buildtool {

HYP_DECLARE_LOG_CHANNEL(BuildTool);

static const HashMap<HypClassDefinitionType, String> g_startMacroNames = {
    { HypClassDefinitionType::CLASS, "HYP_BEGIN_CLASS" },
    { HypClassDefinitionType::STRUCT, "HYP_BEGIN_STRUCT" },
    { HypClassDefinitionType::ENUM, "HYP_BEGIN_ENUM" }
};

static const HashMap<HypClassDefinitionType, String> g_endMacroNames = {
    { HypClassDefinitionType::CLASS, "HYP_END_CLASS" },
    { HypClassDefinitionType::STRUCT, "HYP_END_STRUCT" },
    { HypClassDefinitionType::ENUM, "HYP_END_ENUM" }
};

FilePath CXXModuleGenerator::GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetCXXOutputDirectory() / relativePath.BasePath() / StringUtil::StripExtension(relativePath.Basename()) + ".generated.cpp";
}

Result CXXModuleGenerator::Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    writer.WriteString(HYP_FORMAT("/* Generated from: {} */\n\n", relativePath));

    writer.WriteString(HYP_FORMAT("#include <{}>\n\n", relativePath));

    for (const Pair<String, HypClassDefinition>& pair : mod.GetHypClasses())
    {
        const HypClassDefinition& hypClass = pair.second;

        const bool isComponent = hypClass.HasAttribute("component");
        const bool isEntity = analyzer.HasBaseClass(hypClass, "Entity");
        const bool hasScriptableMethods = hypClass.HasScriptableMethods();

        const HypClassAttributeValue& structSizeAttributeValue = hypClass.GetAttribute("size");
        const HypClassAttributeValue& postLoadAttributeValue = hypClass.GetAttribute("postload");

        if (isComponent || isEntity)
        {
            writer.WriteString("#include <scene/ComponentInterface.hpp>\n");
        }

        if (isEntity)
        {
            writer.WriteString("#include <scene/EntityTag.hpp>\n");
        }

        if (hasScriptableMethods)
        {
            writer.WriteString("#include <core/object/managed/ManagedObjectResource.hpp>\n");
            writer.WriteString("\n");
            writer.WriteString("#include <dotnet/Object.hpp>\n");
            writer.WriteString("#include <dotnet/Class.hpp>\n");
            writer.WriteString("#include <dotnet/Method.hpp>\n");
        }

        writer.WriteString("\nnamespace hyperion {\n\n");

        writer.WriteString(HYP_FORMAT("#pragma region {} Reflection Data\n\n", hypClass.name));

        Array<String> classAttributes;

        for (const Pair<String, HypClassAttributeValue>& attributePair : hypClass.attributes)
        {
            const String& name = attributePair.first;
            const HypClassAttributeValue& value = attributePair.second;

            classAttributes.PushBack(HYP_FORMAT("HypClassAttribute(\"{}\", {})", name.ToLower(), value.ToString()));
        }

        writer.WriteString(HYP_FORMAT("{}({}, {}, {}", g_startMacroNames.At(hypClass.type), hypClass.name, hypClass.staticIndex, hypClass.numDescendants));

        HashSet<const HypClassDefinition*> baseClassDefinitions;

        if (hypClass.baseClassNames.Any())
        {
            for (const String& baseClassName : hypClass.baseClassNames)
            {
                const HypClassDefinition* baseClassDefinition = analyzer.FindHypClassDefinition(baseClassName);

                if (baseClassDefinition)
                {
                    baseClassDefinitions.Insert(baseClassDefinition);
                }
            }
        }

        if (baseClassDefinitions.Any())
        {
            if (baseClassDefinitions.Size() > 1)
            {
                return HYP_MAKE_ERROR(Error, "Multiple base classes not supported");
            }

            const HypClassDefinition* baseClassDefinition = *baseClassDefinitions.Begin();

            writer.WriteString(HYP_FORMAT(", NAME(\"{}\")", baseClassDefinition->name));
        }
        else
        {
            writer.WriteString(", {}");
        }

        if (classAttributes.Any())
        {
            writer.WriteString(", " + String::Join(classAttributes, ','));
        }

        writer.WriteString(")\n");

        for (SizeType i = 0; i < hypClass.members.Size(); ++i)
        {
            const HypMemberDefinition& member = hypClass.members[i];

            // if (member.type == HypMemberType::TYPE_METHOD && member.HasAttribute("Scriptable")) {
            //     continue;
            // }

            String attributesString;

            if (member.attributes.Any())
            {
                attributesString = "Span<const HypClassAttribute> { {";

                for (SizeType i = 0; i < member.attributes.Size(); i++)
                {
                    const String& name = member.attributes[i].first;
                    const HypClassAttributeValue& value = member.attributes[i].second;

                    attributesString += HYP_FORMAT("HypClassAttribute(\"{}\", {})", name.ToLower(), value.ToString());

                    if (i != member.attributes.Size() - 1)
                    {
                        attributesString += ", ";
                    }
                }

                attributesString += " } }";
            }

            if (member.type == HypMemberType::TYPE_FIELD)
            {
                if (member.cxxType->isStatic)
                {
                    if (!member.cxxType->isConst && !member.cxxType->isConstexpr)
                    {
                        return HYP_MAKE_ERROR(Error, "Static fields must be const or constexpr");
                    }

                    writer.WriteString(HYP_FORMAT("    HypConstant(NAME(HYP_STR({})), &{}::{}", member.friendlyName, hypClass.name, member.name));
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("    HypField(NAME(HYP_STR({})), &{}::{}, offsetof({}, {})", member.friendlyName, hypClass.name, member.name, hypClass.name, member.name));
                }

                if (attributesString.Any())
                {
                    writer.WriteString(", " + attributesString);
                }

                writer.WriteString(")");
            }
            else if (member.type == HypMemberType::TYPE_METHOD)
            {
                writer.WriteString(HYP_FORMAT("    HypMethod(NAME(HYP_STR({})), &{}::{}", member.name, hypClass.name, member.name));

                if (attributesString.Any())
                {
                    writer.WriteString(", " + attributesString);
                }

                writer.WriteString(")");
            }
            else if (member.type == HypMemberType::TYPE_PROPERTY)
            {
                String propertyArgsString;

                if (member.attributes.Any())
                {
                    for (SizeType i = 0; i < member.attributes.Size(); i++)
                    {
                        propertyArgsString += member.attributes[i].first;

                        if (i != member.attributes.Size() - 1)
                        {
                            propertyArgsString += ", ";
                        }
                    }
                }

                writer.WriteString(HYP_FORMAT("    HypProperty(NAME(HYP_STR({})){})", member.name, propertyArgsString.Any() ? ", " + propertyArgsString : ""));
            }
            else if (member.type == HypMemberType::TYPE_CONSTANT)
            {
                writer.WriteString(HYP_FORMAT("    HypConstant(NAME(HYP_STR({})), {}::{})", member.friendlyName, hypClass.name, member.name));
            }

            if (i != hypClass.members.Size() - 1)
            {
                writer.WriteString(",");
            }

            writer.WriteString("\n");
        }

        writer.WriteString(HYP_FORMAT("{}\n\n", g_endMacroNames.At(hypClass.type)));

        writer.WriteString(HYP_FORMAT("#pragma endregion {} Reflection Data\n\n", hypClass.name));

        if (hasScriptableMethods)
        {
            writer.WriteString(HYP_FORMAT("#pragma region {} Scriptable Methods\n\n", hypClass.name));

            for (const HypMemberDefinition& member : hypClass.members)
            {
                if (member.type == HypMemberType::TYPE_METHOD && member.HasAttribute("Scriptable"))
                {
                    if (!member.cxxType)
                    {
                        return HYP_MAKE_ERROR(Error, "Missing C++ type for member; Parsing failed");
                    }

                    if (!member.cxxType->isFunction || member.cxxType->isStatic)
                    {
                        return HYP_MAKE_ERROR(Error, "Scriptable attribute can only be applied to instance methods");
                    }

                    const ASTFunctionType* functionType = dynamic_cast<const ASTFunctionType*>(member.cxxType.Get());

                    if (!functionType)
                    {
                        return HYP_MAKE_ERROR(Error, "Internal error: failed cast to ASTFunctionType");
                    }

                    String methodArgsStringSig;
                    String methodArgsStringCall;

                    for (SizeType i = 0; i < functionType->parameters.Size(); ++i)
                    {
                        methodArgsStringSig += functionType->parameters[i]->type->FormatDecl(functionType->parameters[i]->name);
                        methodArgsStringCall += functionType->parameters[i]->name;

                        if (i != functionType->parameters.Size() - 1)
                        {
                            methodArgsStringSig += ", ";
                            methodArgsStringCall += ", ";
                        }
                    }

                    String returnTypeString = functionType->returnType->Format();

                    if (functionType->returnType->IsVoid())
                    {
                        writer.WriteString(HYP_FORMAT("void {}::{}({}){}", hypClass.name, member.name, methodArgsStringSig, functionType->isConstMethod ? " const" : ""));
                        writer.WriteString("\n");
                        writer.WriteString("{\n");
                        writer.WriteString("    if (ManagedObjectResource *managed_object_resource = GetManagedObjectResource()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString("            TResourceHandle<ManagedObjectResource> resource_handle(*managed_object_resource);\n");
                        writer.WriteString("            dotnet::Object *managed_object = managed_object_resource->GetManagedObject();\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("            managed_object->InvokeMethod<void>(method_ptr{});\n", methodArgsStringCall.Any() ? ", " + methodArgsStringCall : ""));
                        writer.WriteString("            return;\n");
                        writer.WriteString("        }\n");
                        // writer.WriteString(HYP_FORMAT("        HYP_FAIL(\"No method '{}' on managed object of type %s\", managed_object->GetClass()->GetName().Data());\n", member.name));
                        writer.WriteString("    }\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("    {}_Impl({});\n", member.name, methodArgsStringCall));
                        writer.WriteString("}\n");
                    }
                    else
                    {
                        writer.WriteString(HYP_FORMAT("{} {}::{}({}){}", returnTypeString, hypClass.name, member.name, methodArgsStringSig, functionType->isConstMethod ? " const" : ""));
                        writer.WriteString("\n");
                        writer.WriteString("{\n");
                        writer.WriteString("    if (ManagedObjectResource *managed_object_resource = GetManagedObjectResource()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString("            TResourceHandle<ManagedObjectResource> resource_handle(*managed_object_resource);\n");
                        writer.WriteString("            dotnet::Object *managed_object = managed_object_resource->GetManagedObject();\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("            return managed_object->InvokeMethod<{}>(method_ptr{});\n", returnTypeString, methodArgsStringCall.Any() ? ", " + methodArgsStringCall : ""));
                        writer.WriteString("        }\n");
                        // writer.WriteString(HYP_FORMAT("        HYP_FAIL(\"No method '{}' on managed object of type %s\", managed_object->GetClass()->GetName().Data());\n", member.name));
                        writer.WriteString("    }\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("    return {}_Impl({});\n", member.name, methodArgsStringCall));
                        writer.WriteString("}\n");
                    }
                }
            }

            writer.WriteString(HYP_FORMAT("#pragma endregion {} Scriptable Methods\n", hypClass.name));
        }

        if (isComponent)
        {
            writer.WriteString(HYP_FORMAT("HYP_REGISTER_COMPONENT({});\n", hypClass.name));
        }

        if (isEntity)
        {
            writer.WriteString(HYP_FORMAT("HYP_REGISTER_ENTITY_TYPE({});\n", hypClass.name));
        }

        if (structSizeAttributeValue.IsValid())
        {
            writer.WriteString(HYP_FORMAT("static_assert(sizeof({}) == {}, \"Expected sizeof({}) to be {} bytes\");\n", hypClass.name, structSizeAttributeValue.ToString(), hypClass.name, structSizeAttributeValue.ToString()));
        }

        if (postLoadAttributeValue.IsValid())
        {
            writer.WriteString(HYP_FORMAT("static const HypClassCallbackRegistration<HypClassCallbackType::ON_POST_LOAD> g_post_load_{}(TypeId::ForType<{}>(), ValueWrapper<{}>());\n", hypClass.name, hypClass.name, postLoadAttributeValue.GetString()));
        }

        writer.WriteString("} // namespace hyperion\n\n");
    }

    return {};
}

} // namespace buildtool
} // namespace hyperion