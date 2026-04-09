/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/CXXModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/containers/HashSet.hpp>

#include <Core/utilities/Format.hpp>

#include <Core/logging/Logger.hpp>

#include <Core/utilities/StringUtil.hpp>

#include <util/Util.hpp>

namespace Hyperion {
namespace CodeGen {

HYP_DECLARE_LOG_CHANNEL(Tool);

static constexpr bool AddIncludesForDependencies = true;

static const HashMap<ClassDefinitionType, String> s_startMacroNames = {
    { ClassDefinitionType::Class, "HYP_BEGIN_CLASS" },
    { ClassDefinitionType::Struct, "HYP_BEGIN_STRUCT" },
    { ClassDefinitionType::Enum, "HYP_BEGIN_ENUM" }
};

static const HashMap<ClassDefinitionType, String> s_endMacroNames = {
    { ClassDefinitionType::Class, "HYP_END_CLASS" },
    { ClassDefinitionType::Struct, "HYP_END_STRUCT" },
    { ClassDefinitionType::Enum, "HYP_END_ENUM" }
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

Result CXXModuleGenerator::GenerateClassDeclHeader(const Analyzer& analyzer, ByteWriter& writer) const
{
    writer.WriteString("class Class;\n\n");

    // writer.WriteString("template <class T>\n");
    // writer.WriteString("struct ClassDecl;\n\n");

    struct ClassInfo
    {
        const ClassDefinition* definition;
    };

    Array<ClassInfo> allClasses;
    HashSet<String> processedNames;

    for (const auto& it : analyzer.GetBuiltinClasses())
    {
        if (processedNames.Contains(it.second.name))
        {
            continue;
        }

        processedNames.Insert(it.second.name);
        allClasses.PushBack({ &it.second });
    }

    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, ClassDefinition>& pair : mod->GetClasses())
        {
            if (processedNames.Contains(pair.second.name))
            {
                continue;
            }

            processedNames.Insert(pair.second.name);
            allClasses.PushBack({ &pair.second });
        }
    }

    std::sort(allClasses.Begin(), allClasses.End(), [](const ClassInfo& a, const ClassInfo& b)
        {
            return a.definition->name < b.definition->name;
        });

    /*for (const ClassInfo& classInfo : allClasses)
    {
        const ClassDefinition& cls = *classInfo.definition;

        if (cls.isCXXClass)
        {
            writer.WriteString(HYP_FORMAT("class {};\n", cls.name));
        }
        else if (cls.isCXXStruct)
        {
            writer.WriteString(HYP_FORMAT("struct {};\n", cls.name));
        }
        else if (cls.isCXXEnum || cls.isCXXEnumClass)
        {
            if (cls.isCXXEnumClass)
            {
                if (cls.baseClassNames.Any())
                {
                    writer.WriteString(HYP_FORMAT("enum class {} : {};\n", cls.name, cls.baseClassNames.Front()));
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("enum class {};\n", cls.name));
                }
            }
            else
            {
                if (cls.baseClassNames.Any())
                {
                    writer.WriteString(HYP_FORMAT("enum {} : {};\n", cls.name, cls.baseClassNames.Front()));
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("enum {};\n", cls.name));
                }
            }
        }
        else
        {
            HYP_LOG(Tool, Error, "Unknown C++ type for class '{}', cannot generate forward declaration", cls.name);
        }
    }

    writer.WriteString("\n");*/

    for (const ClassInfo& classInfo : allClasses)
    {
        const ClassDefinition& cls = *classInfo.definition;
        
        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#if {}\n", cls.condition));
        }

        // add endif at end of scope if the class is conditionally defined
        HYP_DEFER({
            if (cls.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("#endif // {}\n", cls.condition));
            }
        });

        Array<String> namespaceParts = cls.namespaceParts;
        namespaceParts.PopFront(); // remove 'Hyperion'

        if (namespaceParts.Any())
        {
            writer.WriteString(HYP_FORMAT("namespace {}", BuildNamespaceString(namespaceParts)) + " { ");
        }

        writer.WriteString(HYP_FORMAT("HYP_API extern const Class* g_cls{};", cls.name));

        if (namespaceParts.Any())
        {
            writer.WriteString(" }\n");
        }
        else
        {
            writer.WriteString("\n");
        }
    }

    return {};
}

Result CXXModuleGenerator::GenerateClassDeclImplementation(const Analyzer& analyzer, ByteWriter& writer) const
{
    writer.WriteString(GetGeneratedFilePreamble(String::empty));
    writer.WriteString("#include <Core/reflection/Class.hpp>\n");
    writer.WriteString("#include <Core/reflection/ObjectMacros.hpp>\n\n");

    writer.WriteString(String("namespace ") + BaseNamespace + " {\n\n");

    // Collect all Class definitions from builtins and modules
    struct ClassInfo
    {
        const ClassDefinition* definition;
    };

    Array<ClassInfo> allClasses;
    HashSet<String> processedNames;

    writer.WriteString("#pragma region Builtins\n\n");

    // Add builtins
    for (const auto& it : analyzer.GetBuiltinClasses())
    {
        if (processedNames.Contains(it.second.name))
        {
            continue;
        }

        processedNames.Insert(it.second.name);
        allClasses.PushBack({ &it.second });
    }

    // Add module classes
    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, ClassDefinition>& pair : mod->GetClasses())
        {
            if (processedNames.Contains(pair.second.name))
            {
                continue;
            }

            processedNames.Insert(pair.second.name);
            allClasses.PushBack({ &pair.second });
        }
    }

    writer.WriteString("#pragma endregion Builtins\n\n");

    writer.WriteString("#pragma region Defining g_clsXXX globals\n\n");

    for (const ClassInfo& classInfo : allClasses)
    {
        const ClassDefinition& cls = *classInfo.definition;

        Array<String> namespaceParts = cls.namespaceParts;

        if (cls.namespaceParts.Any())
        {
            if (namespaceParts[0] == BaseNamespace)
            {
                namespaceParts.PopFront();
            }
        }
        
        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#if {}\n", cls.condition));
        }

        // add endif at end of scope if the class is conditionally defined
        HYP_DEFER({
            if (cls.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("#endif // {}\n", cls.condition));
            }
        });

        if (namespaceParts.Any())
        {
            const String namespaceString = BuildNamespaceString(namespaceParts);
            writer.WriteString(HYP_FORMAT("namespace {}", namespaceString) + " { ");
            writer.WriteString(HYP_FORMAT("const Class* g_cls{} = nullptr;", cls.name));
            writer.WriteString(" }\n");
        }
        else
        {
            writer.WriteString(HYP_FORMAT("const Class* g_cls{} = nullptr;\n", cls.name));
        }
    }

    writer.WriteString("\n#pragma endregion Defining g_clsXXX globals\n\n");

    // build forward decls
    writer.WriteString("#pragma region Forward declarations\n\n");

    for (const ClassInfo& classInfo : allClasses)
    {
        const ClassDefinition& cls = *classInfo.definition;

        if (cls.namespaceParts.Empty() || cls.namespaceParts[0] != BaseNamespace)
        {
            return HYP_MAKE_ERROR(Error, "Class '{}' is not in the '{}' namespace", cls.name, BaseNamespace);
        }
        
        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#if {}\n", cls.condition));
        }

        // add endif at end of scope if the class is conditionally defined
        HYP_DEFER({
            if (cls.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("#endif // {}\n", cls.condition));
            }
        });

        Array<String> namespaceParts = cls.namespaceParts;
        namespaceParts.PopFront(); // remove 'Hyperion'

        if (namespaceParts.Any())
        {
            writer.WriteString(HYP_FORMAT("namespace {}", BuildNamespaceString(namespaceParts)) + " { ");
        }

        if (classInfo.definition->isCXXClass)
        {
            writer.WriteString(HYP_FORMAT("class {};", cls.name));
        }
        else if (classInfo.definition->isCXXStruct)
        {
            writer.WriteString(HYP_FORMAT("struct {};", cls.name));
        }
        else if (classInfo.definition->isCXXEnumClass)
        {
            if (classInfo.definition->baseClassNames.Any())
            {
                writer.WriteString(HYP_FORMAT("enum class {} : {};", cls.name, classInfo.definition->baseClassNames.Front()));
            }
            else
            {
                writer.WriteString(HYP_FORMAT("enum class {};", cls.name));
            }
        }
        else if (classInfo.definition->isCXXEnum)
        {
            if (classInfo.definition->baseClassNames.Any())
            {
                writer.WriteString(HYP_FORMAT("enum {} : {};", cls.name, classInfo.definition->baseClassNames.Front()));
            }
            else
            {
                writer.WriteString(HYP_FORMAT("enum {};", cls.name));
            }
        }
        else
        {
            HYP_LOG(Tool, Error, "Unknown C++ type for class '{}', cannot generate forward declaration", cls.name);

            return HYP_MAKE_ERROR(Error, "Unknown C++ type for class '{}'", cls.name);
        }

        // close namespace
        writer.WriteString(namespaceParts.Any() ? " }\n" : "\n");
    }

    writer.WriteString("\n#pragma endregion Forward declarations\n\n");

    // // GetClassHelper<T>::Get() implementations
    // writer.WriteString("#pragma region GetClassHelper Get implementations\n\n");

    // for (const ClassInfo& classInfo : allClasses)
    // {
    //     const ClassDefinition& cls = *classInfo.definition;

    //     Array<String> namespaceParts = cls.namespaceParts;

    //     if (cls.namespaceParts.Any())
    //     {
    //         if (namespaceParts[0] == BaseNamespace)
    //         {
    //             namespaceParts.PopFront();
    //         }
    //     }

    //     if (namespaceParts.Any())
    //     {
    //         const String namespaceString = BuildNamespaceString(namespaceParts);
    //         writer.WriteString(HYP_FORMAT("template <> const Class* GetClassHelper<{}::{}>::Get()", namespaceString, cls.name) + " { ");
    //         writer.WriteString(HYP_FORMAT("return {}::g_cls{};", namespaceString, cls.name));
    //         writer.WriteString(" }\n");
    //     }
    //     else
    //     {
    //         writer.WriteString(HYP_FORMAT("template <> const Class* GetClassHelper<{}>::Get()", cls.name) + " { ");
    //         writer.WriteString(HYP_FORMAT("return g_cls{};", cls.name));
    //         writer.WriteString(" }\n");
    //     }
    // }

    // writer.WriteString("\n#pragma endregion GetClassHelper Get implementations\n\n");

    // now we need to add a method to be called that initializes all g_clsXXX variables (TClassStaticInit specializations)
    writer.WriteString("\nHYP_API void InitClassDecls()\n{\n");

    for (const ClassInfo& classInfo : allClasses)
    {
        const ClassDefinition& cls = *classInfo.definition;

        String qualifiedName;

        if (cls.namespaceParts.Any() && cls.namespaceParts[0] == BaseNamespace)
        {
            Array<String> ns = cls.namespaceParts;
            ns.PopFront();

            if (ns.Any())
            {
                qualifiedName = String::Join(ns, "::") + "::" + cls.name;
            }
            else
            {
                qualifiedName = cls.name;
            }
        }
        else if (cls.namespaceParts.Any())
        {
            qualifiedName = String::Join(cls.namespaceParts, "::") + "::" + cls.name;
        }
        else
        {
            qualifiedName = cls.name;
        }
        
        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#if {}\n", cls.condition));
        }

        // add endif at end of scope if the class is conditionally defined
        HYP_DEFER({
            if (cls.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("#endif // {}\n", cls.condition));
            }
        });

        writer.WriteString(HYP_FORMAT("    static TClassStaticInit<{}> s_classInit{};\n", qualifiedName, cls.name));
    }

    writer.WriteString("}\n\n");

    writer.WriteString("} // namespace " + String(BaseNamespace) + "\n");

    return {};
}

Result CXXModuleGenerator::GenerateInline(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    // the including .cpp should provide includes for dependencies
    // so we don't add them here
    writer.WriteString("#include <Core/reflection/ObjectMacros.hpp>\n");
    writer.WriteString("#include <Core/reflection/ClassUtils.hpp>\n");

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        const bool isComponent = cls.HasAttribute("component");
        const bool isEntity = analyzer.HasBaseClass(cls, "Entity");
        const bool hasScriptableMethods = cls.HasScriptableMethods();

        const ClassAttributeValue& structSizeAttributeValue = cls.GetAttribute("size");
        const ClassAttributeValue& postLoadAttributeValue = cls.GetAttribute("postload");

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
            writer.WriteString("#include <dotnet/ManagedMethod.hpp>\n");
        }

        writer.WriteString(HYP_FORMAT("using namespace {};\n", BaseNamespace));

        if (cls.namespaceParts.Any() && (cls.namespaceParts.Size() > 1 || cls.namespaceParts[0] != BaseNamespace))
        {
            writer.WriteString(HYP_FORMAT("using {} = {}::{};\n",
                cls.name,
                BuildNamespaceString(cls.namespaceParts),
                cls.name));
        }
        
        writer.WriteString(HYP_FORMAT("using {}::Class;\n", BaseNamespace)); // to resolve ambiguity in intellisense

        writer.WriteString("\n");

        // forward declare g_clsXXX

        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#if {}\n\n", cls.condition));
        }

        // add endif at end of scope if the class is conditionally defined
        HYP_DEFER({
            if (cls.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("#endif // {}\n", cls.condition));
            }
        });

        writer.WriteString(HYP_FORMAT("namespace {}", BuildNamespaceString(cls.namespaceParts)) + " {\n");
        writer.WriteString(HYP_FORMAT("extern const Class* g_cls{};\n", cls.name));
        writer.WriteString("} " + HYP_FORMAT("// namespace {}\n\n", BuildNamespaceString(cls.namespaceParts)));

        if (cls.namespaceParts.Any() && (cls.namespaceParts.Size() > 1 || cls.namespaceParts[0] != BaseNamespace))
        {
            // alias the global g_clsXXX to the namespaced one to avoid having to qualify it everywhere in the generated code
            writer.WriteString(HYP_FORMAT("static const Class*& g_cls{} = {}::g_cls{};\n\n", cls.name, BuildNamespaceString(cls.namespaceParts), cls.name));
        }

        writer.WriteString(HYP_FORMAT("#pragma region {} Reflection Data\n\n", cls.name));

        Array<String> classAttributes;

        for (const Pair<String, ClassAttributeValue>& attributePair : cls.attributes)
        {
            const String& name = attributePair.first;
            const ClassAttributeValue& value = attributePair.second;

            classAttributes.PushBack(HYP_FORMAT("ClassAttribute(\"{}\", {})", name.ToLower(), value.ToString()));
        }

        writer.WriteString(HYP_FORMAT("{}({}, {}, {}", s_startMacroNames.At(cls.type), cls.name, cls.staticIndex, cls.numDescendants));

        HashSet<const ClassDefinition*> baseClassDefinitions;

        if (cls.baseClassNames.Any())
        {
            for (const String& baseClassName : cls.baseClassNames)
            {
                const ClassDefinition* baseClassDefinition = analyzer.FindClassDefinition(baseClassName);

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

            const ClassDefinition* baseClassDefinition = *baseClassDefinitions.Begin();

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
        
        for (size_t i = 0; i < cls.members.Size(); ++i)
        {
            const MemberDef& member = cls.members[i];

            if (member.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("#if {}\n", member.condition));
            }

            String attributesString;

            if (member.attributes.Any())
            {
                attributesString = "Span<const ClassAttribute> { {";

                for (size_t i = 0; i < member.attributes.Size(); i++)
                {
                    const String& name = member.attributes[i].first;
                    const ClassAttributeValue& value = member.attributes[i].second;

                    attributesString += HYP_FORMAT("ClassAttribute(\"{}\", {})", name.ToLower(), value.ToString());

                    if (i != member.attributes.Size() - 1)
                    {
                        attributesString += ", ";
                    }
                }

                attributesString += " } }";
            }

            if (member.type == MemberType::Field)
            {
                if (member.cxxType->isStatic)
                {
                    if (!member.cxxType->isConst && !member.cxxType->isConstexpr)
                    {
                        return HYP_MAKE_ERROR(Error, "Static fields must be const or constexpr");
                    }

                    writer.WriteString(HYP_FORMAT("    StaticField(NAME(HYP_STR({})), &{}::{}", member.friendlyName, cls.name, member.name));
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("    Field(NAME(HYP_STR({})), &{}::{}, OffsetOf(&{}::{})", member.friendlyName, cls.name, member.name, cls.name, member.name));
                }

                if (attributesString.Any())
                {
                    writer.WriteString(", " + attributesString);
                }

                writer.WriteString(")");
            }
            else if (member.type == MemberType::Method)
            {
                writer.WriteString(HYP_FORMAT("    Method(NAME(HYP_STR({})), &{}::{}", member.name, cls.name, member.name));

                if (attributesString.Any())
                {
                    writer.WriteString(", " + attributesString);
                }

                writer.WriteString(")");
            }
            else if (member.type == MemberType::Property)
            {
                String propertyArgsString;

                if (member.attributes.Any())
                {
                    for (size_t i = 0; i < member.attributes.Size(); i++)
                    {
                        propertyArgsString += member.attributes[i].first;

                        if (i != member.attributes.Size() - 1)
                        {
                            propertyArgsString += ", ";
                        }
                    }
                }

                writer.WriteString(HYP_FORMAT("    Property(NAME(HYP_STR({})){})", member.name, propertyArgsString.Any() ? ", " + propertyArgsString : ""));
            }
            else if (member.type == MemberType::StaticField)
            {
                writer.WriteString(HYP_FORMAT("    StaticField(NAME(HYP_STR({})), {}::{})", member.friendlyName, cls.name, member.name));
            }

            if (i != cls.members.Size() - 1)
            {
                writer.WriteString(",");
            }

            writer.WriteString("\n");

            if (member.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("#endif // {}\n", member.condition));
            }
        }

        writer.WriteString(HYP_FORMAT("{}\n\n", s_endMacroNames.At(cls.type)));

        writer.WriteString(HYP_FORMAT("#pragma endregion {} Reflection Data\n\n", cls.name));

        if (hasScriptableMethods)
        {
            writer.WriteString(HYP_FORMAT("#pragma region {} Scriptable Methods\n\n", cls.name));

            for (const MemberDef& member : cls.members)
            {
                if (member.type == MemberType::Method && member.HasAttribute("Scriptable"))
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

                    for (size_t i = 0; i < functionType->parameters.Size(); ++i)
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
                        writer.WriteString(HYP_FORMAT("void {}::{}({}){}", cls.name, member.name, methodArgsStringSig, functionType->isConstMethod ? " const" : ""));
                        writer.WriteString("\n");
                        writer.WriteString("{\n");
                        writer.WriteString("#ifdef HYP_DOTNET\n");
                        writer.WriteString("    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString("            TResourceGuard<ScriptObjectResource> resourceGuard(*managed_object_resource);\n");
                        writer.WriteString("            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("            managed_object->InvokeMethod<void>(method_ptr{});\n", methodArgsStringCall.Any() ? ", " + methodArgsStringCall : ""));
                        writer.WriteString("            return;\n");
                        writer.WriteString("        }\n");
                        writer.WriteString("    }\n");
                        writer.WriteString("\n");
                        writer.WriteString("#endif // HYP_DOTNET\n");
                        writer.WriteString("\n"); /// \todo : support HYP_SCRIPT here
                        writer.WriteString(HYP_FORMAT("    {}_Impl({});\n", member.name, methodArgsStringCall));
                        writer.WriteString("}\n");
                    }
                    else
                    {
                        writer.WriteString(HYP_FORMAT("{} {}::{}({}){}", returnTypeString, cls.name, member.name, methodArgsStringSig, functionType->isConstMethod ? " const" : ""));
                        writer.WriteString("\n");
                        writer.WriteString("{\n");
                        writer.WriteString("#ifdef HYP_DOTNET\n");
                        writer.WriteString("    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString("            TResourceGuard<ScriptObjectResource> resourceGuard(*managed_object_resource);\n");
                        writer.WriteString("            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("            return managed_object->InvokeMethod<{}>(method_ptr{});\n", returnTypeString, methodArgsStringCall.Any() ? ", " + methodArgsStringCall : ""));
                        writer.WriteString("        }\n");
                        writer.WriteString("    }\n");
                        writer.WriteString("\n");
                        writer.WriteString("#endif // HYP_DOTNET\n");
                        writer.WriteString("\n"); /// \todo : support HYP_SCRIPT here
                        writer.WriteString(HYP_FORMAT("    return {}_Impl({});\n", member.name, methodArgsStringCall));
                        writer.WriteString("}\n");
                    }
                }
            }

            writer.WriteString(HYP_FORMAT("#pragma endregion {} Scriptable Methods\n", cls.name));
        }

        if (isComponent)
        {
            writer.WriteString(HYP_FORMAT("HYP_REGISTER_COMPONENT({});\n", cls.name));
        }

        if (isEntity)
        {
            writer.WriteString(HYP_FORMAT("HYP_REGISTER_ENTITY_TYPE({});\n", cls.name));
        }

        if (structSizeAttributeValue.IsValid())
        {
            writer.WriteString(HYP_FORMAT("static_assert(sizeof({}) == {}, \"Expected sizeof({}) to be {} bytes\");\n", cls.name, structSizeAttributeValue.ToString(), cls.name, structSizeAttributeValue.ToString()));
        }

        if (postLoadAttributeValue.IsValid())
        {
            writer.WriteString(HYP_FORMAT("static const ClassCallbackRegistration<ClassCallbackType::ON_POST_LOAD> g_post_load_{}(TypeId::ForType<{}>(), ValueWrapper<{}>());\n", cls.name, cls.name, postLoadAttributeValue.GetString()));
        }
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

        writer.WriteString(HYP_FORMAT("/* Generated from: {} */\n\n", relativePath).ReplaceAll("\\", "/"));

        addInclude(relativePath);
    }

    if constexpr (AddIncludesForDependencies)
    {
        if (mod.GetDependencyModules().Any())
        {
            for (Module* dependencyModule : mod.GetDependencyModules())
            {
                Assert(dependencyModule != nullptr);

                if (!dependencyModule->GetPath().Any())
                {
                    HYP_LOG(Tool, Error, "Dependency module has no path! Skipping include.");

                    continue;
                }

                addInclude(FilePath::Relative(dependencyModule->GetPath(), analyzer.GetSourceDirectory()));
            }

            writer.WriteString("\n");
        }
    }

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        const bool isComponent = cls.HasAttribute("component");
        const bool isEntity = analyzer.HasBaseClass(cls, "Entity");
        const bool hasScriptableMethods = cls.HasScriptableMethods();

        const ClassAttributeValue& structSizeAttributeValue = cls.GetAttribute("size");
        const ClassAttributeValue& postLoadAttributeValue = cls.GetAttribute("postload");

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
            addInclude("dotnet/ManagedMethod.hpp");
        }

        writer.WriteString(HYP_FORMAT("using namespace {};\n", BaseNamespace));

        if (cls.namespaceParts.Any() && (cls.namespaceParts.Size() > 1 || cls.namespaceParts[0] != BaseNamespace))
        {
            // bring the class into the current namespace to avoid having to qualify it everywhere in the generated code
            writer.WriteString(HYP_FORMAT("using {} = {}::{};\n",
                cls.name,
                BuildNamespaceString(cls.namespaceParts),
                cls.name));
        }
        
        writer.WriteString(HYP_FORMAT("using {}::Class;\n", BaseNamespace)); // to resolve ambiguity in intellisense

        writer.WriteString("\n");

        // forward declare g_clsXXX
        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#if {}\n\n", cls.condition));
        }

        // add endif at end of scope if the class is conditionally defined
        HYP_DEFER({
            if (cls.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("#endif // {}\n", cls.condition));
            }
        });

        writer.WriteString(HYP_FORMAT("namespace {}", BuildNamespaceString(cls.namespaceParts)) + " {\n");
        writer.WriteString(HYP_FORMAT("extern const Class* g_cls{};\n", cls.name));
        writer.WriteString("} " + HYP_FORMAT("// namespace {}\n\n", BuildNamespaceString(cls.namespaceParts)));
        
        if (cls.namespaceParts.Any() && (cls.namespaceParts.Size() > 1 || cls.namespaceParts[0] != BaseNamespace))
        {
            // alias the global g_clsXXX to the namespaced one to avoid having to qualify it everywhere in the generated code
            writer.WriteString(HYP_FORMAT("static const Class*& g_cls{} = {}::g_cls{};\n\n", cls.name, BuildNamespaceString(cls.namespaceParts), cls.name));
        }

        writer.WriteString(HYP_FORMAT("#pragma region {} Reflection Data\n\n", cls.name));

        Array<String> classAttributes;

        for (const Pair<String, ClassAttributeValue>& attributePair : cls.attributes)
        {
            const String& name = attributePair.first;
            const ClassAttributeValue& value = attributePair.second;

            classAttributes.PushBack(HYP_FORMAT("ClassAttribute(\"{}\", {})", name.ToLower(), value.ToString()));
        }

        writer.WriteString(HYP_FORMAT("{}({}, {}, {}", s_startMacroNames.At(cls.type), cls.name, cls.staticIndex, cls.numDescendants));

        HashSet<const ClassDefinition*> baseClassDefinitions;

        if (cls.baseClassNames.Any())
        {
            for (const String& baseClassName : cls.baseClassNames)
            {
                const ClassDefinition* baseClassDefinition = analyzer.FindClassDefinition(baseClassName);

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

            const ClassDefinition* baseClassDefinition = *baseClassDefinitions.Begin();

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

        for (size_t i = 0; i < cls.members.Size(); ++i)
        {
            const MemberDef& member = cls.members[i];

            if (member.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("#if {}\n", member.condition));
            }

            // if (member.type == MemberType::Method && member.HasAttribute("Scriptable")) {
            //     continue;
            // }

            String attributesString;

            if (member.attributes.Any())
            {
                attributesString = "Span<const ClassAttribute> { {";

                for (size_t i = 0; i < member.attributes.Size(); i++)
                {
                    const String& name = member.attributes[i].first;
                    const ClassAttributeValue& value = member.attributes[i].second;

                    attributesString += HYP_FORMAT("ClassAttribute(\"{}\", {})", name.ToLower(), value.ToString());

                    if (i != member.attributes.Size() - 1)
                    {
                        attributesString += ", ";
                    }
                }

                attributesString += " } }";
            }

            if (member.type == MemberType::Field)
            {
                if (member.cxxType->isStatic)
                {
                    if (!member.cxxType->isConst && !member.cxxType->isConstexpr)
                    {
                        return HYP_MAKE_ERROR(Error, "Static fields must be const or constexpr");
                    }

                    writer.WriteString(HYP_FORMAT("    StaticField(NAME(HYP_STR({})), &{}::{}", member.friendlyName, cls.name, member.name));
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("    Field(NAME(HYP_STR({})), &{}::{}, OffsetOf(&{}::{})", member.friendlyName, cls.name, member.name, cls.name, member.name));
                }

                if (attributesString.Any())
                {
                    writer.WriteString(", " + attributesString);
                }

                writer.WriteString(")");
            }
            else if (member.type == MemberType::Method)
            {
                writer.WriteString(HYP_FORMAT("    Method(NAME(HYP_STR({})), &{}::{}", member.name, cls.name, member.name));

                if (attributesString.Any())
                {
                    writer.WriteString(", " + attributesString);
                }

                writer.WriteString(")");
            }
            else if (member.type == MemberType::Property)
            {
                String propertyArgsString;

                if (member.attributes.Any())
                {
                    for (size_t i = 0; i < member.attributes.Size(); i++)
                    {
                        propertyArgsString += member.attributes[i].first;

                        if (i != member.attributes.Size() - 1)
                        {
                            propertyArgsString += ", ";
                        }
                    }
                }

                writer.WriteString(HYP_FORMAT("    Property(NAME(HYP_STR({})){})", member.name, propertyArgsString.Any() ? ", " + propertyArgsString : ""));
            }
            else if (member.type == MemberType::StaticField)
            {
                writer.WriteString(HYP_FORMAT("    StaticField(NAME(HYP_STR({})), {}::{})", member.friendlyName, cls.name, member.name));
            }

            if (i != cls.members.Size() - 1)
            {
                writer.WriteString(",");
            }

            writer.WriteString("\n");
        }

        writer.WriteString(HYP_FORMAT("{}\n\n", s_endMacroNames.At(cls.type)));

        writer.WriteString(HYP_FORMAT("#pragma endregion {} Reflection Data\n\n", cls.name));

        if (hasScriptableMethods)
        {
            writer.WriteString(HYP_FORMAT("#pragma region {} Scriptable Methods\n\n", cls.name));

            for (const MemberDef& member : cls.members)
            {
                if (member.type == MemberType::Method && member.HasAttribute("Scriptable"))
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

                    for (size_t i = 0; i < functionType->parameters.Size(); ++i)
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
                        writer.WriteString(HYP_FORMAT("void {}::{}({}){}", cls.name, member.name, methodArgsStringSig, functionType->isConstMethod ? " const" : ""));
                        writer.WriteString("\n");
                        writer.WriteString("{\n");
                        writer.WriteString("#ifdef HYP_DOTNET\n");
                        writer.WriteString("    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString("            TResourceGuard<ScriptObjectResource> resourceGuard(*managed_object_resource);\n");
                        writer.WriteString("            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("            managed_object->InvokeMethod<void>(method_ptr{});\n", methodArgsStringCall.Any() ? ", " + methodArgsStringCall : ""));
                        writer.WriteString("            return;\n");
                        writer.WriteString("        }\n");
                        // writer.WriteString(HYP_FORMAT("        HYP_FAIL(\"No method '{}' on managed object of type %s\", managed_object->GetClass()->GetName().Data());\n", member.name));
                        writer.WriteString("    }\n");
                        writer.WriteString("#endif // HYP_DOTNET\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("    {}_Impl({});\n", member.name, methodArgsStringCall));
                        writer.WriteString("}\n");
                    }
                    else
                    {
                        writer.WriteString(HYP_FORMAT("{} {}::{}({}){}", returnTypeString, cls.name, member.name, methodArgsStringSig, functionType->isConstMethod ? " const" : ""));
                        writer.WriteString("\n");
                        writer.WriteString("{\n");
                        writer.WriteString("#ifdef HYP_DOTNET\n");
                        writer.WriteString("    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString("            TResourceGuard<ScriptObjectResource> resourceGuard(*managed_object_resource);\n");
                        writer.WriteString("            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("            return managed_object->InvokeMethod<{}>(method_ptr{});\n", returnTypeString, methodArgsStringCall.Any() ? ", " + methodArgsStringCall : ""));
                        writer.WriteString("        }\n");
                        // writer.WriteString(HYP_FORMAT("        HYP_FAIL(\"No method '{}' on managed object of type %s\", managed_object->GetClass()->GetName().Data());\n", member.name));
                        writer.WriteString("    }\n");
                        writer.WriteString("#endif // HYP_DOTNET\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("    return {}_Impl({});\n", member.name, methodArgsStringCall));
                        writer.WriteString("}\n");
                    }
                }
            }

            writer.WriteString(HYP_FORMAT("#pragma endregion {} Scriptable Methods\n", cls.name));
        }

        if (isComponent)
        {
            writer.WriteString(HYP_FORMAT("HYP_REGISTER_COMPONENT({});\n", cls.name));
        }

        if (isEntity)
        {
            writer.WriteString(HYP_FORMAT("HYP_REGISTER_ENTITY_TYPE({});\n", cls.name));
        }

        if (structSizeAttributeValue.IsValid())
        {
            writer.WriteString(HYP_FORMAT("static_assert(sizeof({}) == {}, \"Expected sizeof({}) to be {} bytes\");\n", cls.name, structSizeAttributeValue.ToString(), cls.name, structSizeAttributeValue.ToString()));
        }

        if (postLoadAttributeValue.IsValid())
        {
            writer.WriteString(HYP_FORMAT("static const ClassCallbackRegistration<ClassCallbackType::ON_POST_LOAD> g_post_load_{}(TypeId::ForType<{}>(), ValueWrapper<{}>());\n", cls.name, cls.name, postLoadAttributeValue.GetString()));
        }
    }

    return {};
}

} // namespace CodeGen
} // namespace Hyperion