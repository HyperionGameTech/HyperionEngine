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

#include <util/ParseUtil.hpp>

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

FilePath CXXModuleGenerator::GetInlineOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    // Flatten .inl files into the root of the CXX output directory so they can be included as <Foo.generated.inl>
    return analyzer.GetCXXOutputDirectory() / StringUtil::StripExtension(relativePath.Basename()) + ".generated.inl";
}

Result CXXModuleGenerator::GenerateHypClassDeclHeader(const Analyzer& analyzer, ByteWriter& writer) const
{
    writer.WriteString("class HypClass;\n\n");

    // writer.WriteString("template <class T>\n");
    // writer.WriteString("struct HypClassDecl;\n\n");

    struct ClassInfo
    {
        const HypClassDefinition* definition;
        String namespacePath;
    };

    Array<ClassInfo> allClasses;
    HashSet<String> processedNames;

    for (const auto& it : analyzer.GetBuiltinHypClasses())
    {
        if (processedNames.Contains(it.second.name))
        {
            continue;
        }

        processedNames.Insert(it.second.name);
        allClasses.PushBack({ &it.second, "hyperion" });
    }

    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, HypClassDefinition>& pair : mod->GetHypClasses())
        {
            if (processedNames.Contains(pair.second.name))
            {
                continue;
            }

            processedNames.Insert(pair.second.name);
            allClasses.PushBack({ &pair.second, "hyperion" });
        }
    }

    std::sort(allClasses.Begin(), allClasses.End(), [](const ClassInfo& a, const ClassInfo& b)
        {
            return a.definition->name < b.definition->name;
        });

    /*for (const ClassInfo& classInfo : allClasses)
    {
        const HypClassDefinition& hypClass = *classInfo.definition;

        if (hypClass.isCXXClass)
        {
            writer.WriteString(HYP_FORMAT("class {};\n", hypClass.name));
        }
        else if (hypClass.isCXXStruct)
        {
            writer.WriteString(HYP_FORMAT("struct {};\n", hypClass.name));
        }
        else if (hypClass.isCXXEnum || hypClass.isCXXEnumClass)
        {
            if (hypClass.isCXXEnumClass)
            {
                if (hypClass.baseClassNames.Any())
                {
                    writer.WriteString(HYP_FORMAT("enum class {} : {};\n", hypClass.name, hypClass.baseClassNames.Front()));
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("enum class {};\n", hypClass.name));
                }
            }
            else
            {
                if (hypClass.baseClassNames.Any())
                {
                    writer.WriteString(HYP_FORMAT("enum {} : {};\n", hypClass.name, hypClass.baseClassNames.Front()));
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("enum {};\n", hypClass.name));
                }
            }
        }
        else
        {
            HYP_LOG(BuildTool, Error, "Unknown C++ type for class '{}', cannot generate forward declaration", hypClass.name);
        }
    }

    writer.WriteString("\n");*/

    for (const ClassInfo& classInfo : allClasses)
    {
        const HypClassDefinition& hypClass = *classInfo.definition;

        writer.WriteString(HYP_FORMAT("HYP_API extern const HypClass* g_cls{};\n", hypClass.name));
        // writer.WriteString(HYP_FORMAT("template <>\n"));
        // writer.WriteString(HYP_FORMAT("struct HypClassDecl<{}>\n", hypClass.name));
        // writer.WriteString("{\n");
        // writer.WriteString(HYP_FORMAT("    using Type = {};\n\n", hypClass.name));
        // writer.WriteString("    static const HypClass** s_pClass;\n");
        // writer.WriteString("};\n\n");
    }

    return {};
}

Result CXXModuleGenerator::GenerateHypClassDeclImplementation(const Analyzer& analyzer, ByteWriter& writer) const
{
    writer.WriteString(GetGeneratedFilePreamble(String::empty));
    writer.WriteString("#include <core/reflection/HypClass.hpp>\n\n");

    writer.WriteString("namespace hyperion {\n\n");
    writer.WriteString("#include <HypClassDecls.inc>\n");

    // Collect all HypClass definitions from builtins and modules
    struct ClassInfo
    {
        const HypClassDefinition* definition;
        String namespacePath;
    };

    Array<ClassInfo> allClasses;
    HashSet<String> processedNames;

    // Add builtins
    for (const auto& it : analyzer.GetBuiltinHypClasses())
    {
        if (processedNames.Contains(it.second.name))
        {
            continue;
        }

        processedNames.Insert(it.second.name);
        allClasses.PushBack({ &it.second, "hyperion" });
    }

    // Add module classes
    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, HypClassDefinition>& pair : mod->GetHypClasses())
        {
            if (processedNames.Contains(pair.second.name))
            {
                continue;
            }

            processedNames.Insert(pair.second.name);
            allClasses.PushBack({ &pair.second, "hyperion" });
        }
    }

    for (const ClassInfo& classInfo : allClasses)
    {
        const HypClassDefinition& hypClass = *classInfo.definition;

        writer.WriteString(HYP_FORMAT("const HypClass* g_cls{} = nullptr;\n", hypClass.name));
        // writer.WriteString(HYP_FORMAT("const HypClass** HypClassDecl<{}>::s_pClass = nullptr;\n\n", hypClass.name));
        // writer.WriteString(HYP_FORMAT("static struct HypClassDeclInitializer_{}\n", hypClass.name));
        // writer.WriteString("{\n");
        // writer.WriteString(HYP_FORMAT("    HypClassDeclInitializer_{}()\n", hypClass.name));
        // writer.WriteString("    {\n");
        // writer.WriteString(HYP_FORMAT("        HypClassDecl<{}>::s_pClass = &g_cls{};\n", hypClass.name, hypClass.name));
        // writer.WriteString("    }\n");
        // writer.WriteString("} " + HYP_FORMAT(" g_hypClassDeclInitializer_{};\n\n", hypClass.name));
    }

    writer.WriteString("} // namespace hyperion\n");

    return {};
}

Result CXXModuleGenerator::GenerateInline(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    // the including .cpp should provide includes for dependencies
    // so we don't add them here
    writer.WriteString("#include <core/reflection/HypObjectMacros.hpp>\n");
    writer.WriteString("#include <core/reflection/HypClassUtils.hpp>\n");

    for (const Pair<String, HypClassDefinition>& pair : mod.GetHypClasses())
    {
        const HypClassDefinition& hypClass = pair.second;

        const bool isComponent = hypClass.HasAttribute("component");
        const bool isEntity = analyzer.HasBaseClass(hypClass, "Entity");
        const bool hasScriptableMethods = hypClass.HasScriptableMethods();

        const HypClassAttributeValue& structSizeAttributeValue = hypClass.GetAttribute("size");
        const HypClassAttributeValue& postLoadAttributeValue = hypClass.GetAttribute("postload");

        // Add minimal feature-specific includes when needed
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
            writer.WriteString("#include <scripting/ScriptObjectResource.hpp>\n");
            writer.WriteString("#include <dotnet/ManagedObject.hpp>\n");
            writer.WriteString("#include <dotnet/ManagedClass.hpp>\n");
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
                        writer.WriteString("    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString("            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);\n");
                        writer.WriteString("            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("            managed_object->InvokeMethod<void>(method_ptr{});\n", methodArgsStringCall.Any() ? ", " + methodArgsStringCall : ""));
                        writer.WriteString("            return;\n");
                        writer.WriteString("        }\n");
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
                        writer.WriteString("    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString("            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);\n");
                        writer.WriteString("            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("            return managed_object->InvokeMethod<{}>(method_ptr{});\n", returnTypeString, methodArgsStringCall.Any() ? ", " + methodArgsStringCall : ""));
                        writer.WriteString("        }\n");
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

Result CXXModuleGenerator::Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    HashSet<String> addedIncludes;
    const auto addInclude = [&writer, &addedIncludes](const String& include)
    {
        if (addedIncludes.Contains(include))
        {
            return;
        }

        addedIncludes.Insert(include);

        writer.WriteString(HYP_FORMAT("#include <{}>\n", include.ReplaceAll("\\", "/")));
    };

    if (mod.GetPath().Any())
    {
        FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

        writer.WriteString(HYP_FORMAT("/* Generated from: {} */\n\n", relativePath));

        addInclude(relativePath);
    }

    if (mod.GetDependencyModules().Any())
    {
        for (Module* dependencyModule : mod.GetDependencyModules())
        {
            Assert(dependencyModule != nullptr);

            if (!dependencyModule->GetPath().Any())
            {
                HYP_LOG(BuildTool, Error, "Dependency module has no path! Skipping include.");

                continue;
            }

            addInclude(FilePath::Relative(dependencyModule->GetPath(), analyzer.GetSourceDirectory()));
        }

        writer.WriteString("\n");
    }

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
            addInclude("scene/ComponentInterface.hpp");
        }

        if (isEntity)
        {
            addInclude("scene/EntityTag.hpp");
        }

        if (hasScriptableMethods)
        {
            addInclude("scripting/ScriptObjectResource.hpp");
            writer.WriteString("\n");
            addInclude("dotnet/ManagedObject.hpp");
            addInclude("dotnet/ManagedClass.hpp");
            addInclude("dotnet/Method.hpp");
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
                        writer.WriteString("    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString("            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);\n");
                        writer.WriteString("            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();\n");
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
                        writer.WriteString("    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString("            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);\n");
                        writer.WriteString("            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();\n");
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