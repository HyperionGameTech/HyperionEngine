/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <generator/generators/CXXModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Containers/Set.hpp>

#include <Core/Utilities/Format.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <util/Util.hpp>

namespace Hyperion {
namespace CodeGen {

HYP_DECLARE_LOG_CHANNEL(Tool);

static constexpr bool AddIncludesForDependencies = true;

static const Map<ClassDefinitionType, String> s_startMacroNames = {
    { ClassDefinitionType::Class, "HYP_BEGIN_CLASS" },
    { ClassDefinitionType::Struct, "HYP_BEGIN_STRUCT" },
    { ClassDefinitionType::Enum, "HYP_BEGIN_ENUM" }
};

static const Map<ClassDefinitionType, String> s_endMacroNames = {
    { ClassDefinitionType::Class, "HYP_END_CLASS" },
    { ClassDefinitionType::Struct, "HYP_END_STRUCT" },
    { ClassDefinitionType::Enum, "HYP_END_ENUM" }
};

static const Array<ModuleAPIMapping> s_moduleAPIMappings = {
    { "Core", "CORE_API", "Core", "Core" },
    { "Editor", "EDITOR_API", "Editor", "Editor" },
    { "Lang", "SCRIPT_API", "Lang", "Script" }
};

const Array<ModuleAPIMapping>& CXXModuleGenerator::GetModuleAPIMappings()
{
    return s_moduleAPIMappings;
}

static const ModuleAPIMapping s_engineAPIMapping = { "", "ENGINE_API", "", "Engine" };

ModuleAPIMapping CXXModuleGenerator::GetMappingForModule(const Analyzer& analyzer, const Module& mod)
{
    if (!mod.GetPath().Any())
    {
        return s_moduleAPIMappings[0];
    }

    // a built-in plugin's classes are part of the engine (or editor) library, but each module gets its own ClassDecls and init function
    if (const PluginModuleInfo* pluginModule = analyzer.FindPluginModule(mod.GetPath()))
    {
        return {
            String::empty,
            pluginModule->isEditor ? "EDITOR_API" : "ENGINE_API",
            "Plugins/" + pluginModule->name,
            "Plugin_" + pluginModule->name,
            pluginModule->name
        };
    }

    const String relativePath = analyzer.GetRelativePath(mod.GetPath()).ReplaceAll("\\", "/");

    for (const ModuleAPIMapping& mapping : s_moduleAPIMappings)
    {
        if (relativePath.StartsWith(mapping.subdirPattern))
        {
            return mapping;
        }
    }

    return s_engineAPIMapping;
}

String CXXModuleGenerator::GetAPIMacroForModule(const Analyzer& analyzer, const Module& mod)
{
    if (analyzer.IsGameModule())
    {
        return String::empty;
    }

    return GetMappingForModule(analyzer, mod).apiMacro;
}

String CXXModuleGenerator::GetInitFunctionName(const ModuleAPIMapping& mapping)
{
    return "InitializeModule_" + mapping.initSuffix;
}

static String GetOutputSubdirForModule(const Analyzer& analyzer, const Module& mod)
{
    if (analyzer.IsGameModule())
    {
        return String::empty;
    }

    return CXXModuleGenerator::GetMappingForModule(analyzer, mod).outputSubdir;
}

FilePath CXXModuleGenerator::GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    const FilePath relativePath = analyzer.GetRelativePath(mod.GetPath());
    const FilePath subdir = GetOutputSubdirForModule(analyzer, mod);

    return analyzer.GetCXXOutputDirectory() / subdir / String(StringUtil::StripExtension(relativePath.Basename())) + ".generated.cpp";
}

FilePath CXXModuleGenerator::GetInlineOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    const FilePath relativePath = analyzer.GetRelativePath(mod.GetPath());
    const FilePath subdir = GetOutputSubdirForModule(analyzer, mod);

    // Flatten .inl files into the root of the CXX output directory so they can be included as <Foo.generated.inl>
    return analyzer.GetCXXOutputDirectory() / subdir / String(StringUtil::StripExtension(relativePath.Basename())) + ".generated.inl";
}

// Currently this ClassDecls header is unused, may disappear soonish.
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
    Set<String> processedNames;

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

        const String apiMacro = cls.declModule != nullptr ? GetAPIMacroForModule(analyzer, *cls.declModule) : "ENGINE_API";
        writer.WriteString(HYP_FORMAT("{} extern const Class* g_cls{};", apiMacro, cls.name));

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

Result CXXModuleGenerator::GenerateClassDeclImplementation(const Analyzer& analyzer, const ModuleAPIMapping& mapping, ByteWriter& writer) const
{
    writer.WriteString(GetGeneratedFilePreamble(String::empty));
    writer.WriteString("#include <Core/Reflection/Class.hpp>\n");
    writer.WriteString("#include <Core/Reflection/ObjectMacros.hpp>\n\n");

    writer.WriteString(String("namespace ") + BaseNamespace + " {\n\n");

    // Collect all Class definitions from builtins and modules that match the given API macro
    struct ClassInfo
    {
        const ClassDefinition* definition;
    };

    Array<ClassInfo> allClasses;
    Set<String> processedNames;

    // Add builtins, routing each to the appropriate API macro
    bool wroteBuiltinsRegion = false;

    for (const auto& it : analyzer.GetBuiltinClasses())
    {
        const bool isCoreBuiltin = (it.second.name == "ObjectBase");
        const String builtinInitSuffix = isCoreBuiltin ? "Core" : "Engine";

        if (builtinInitSuffix != mapping.initSuffix)
        {
            continue;
        }

        if (processedNames.Contains(it.second.name))
        {
            continue;
        }

        if (!wroteBuiltinsRegion)
        {
            writer.WriteString("#pragma region Builtins\n\n");
            wroteBuiltinsRegion = true;
        }

        processedNames.Insert(it.second.name);
        allClasses.PushBack({ &it.second });
    }

    if (wroteBuiltinsRegion)
    {
        writer.WriteString("#pragma endregion Builtins\n\n");
    }

    // Add module classes that belong to the target module group
    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        if (GetMappingForModule(analyzer, *mod).initSuffix != mapping.initSuffix)
        {
            continue;
        }

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

    // a plugin module's entry point, which its PluginModuleRegister.cpp adds to the engine's plugin module initializers
    const auto writePluginModuleEntry = [&writer, &mapping]()
    {
        if (mapping.pluginModuleName.Empty())
        {
            return;
        }

        writer.WriteString(HYP_FORMAT("\n// HYP_DEFINE_PLUGIN_MODULE\nvoid HypRegisterPluginModule_{}();\n\n", mapping.pluginModuleName));
        writer.WriteString(HYP_FORMAT("HYP_EXPORT void HypInitializePluginModule_{}()\n", mapping.pluginModuleName) + "{\n");
        writer.WriteString(String("    ") + BaseNamespace + "::" + GetInitFunctionName(mapping) + "();\n");
        writer.WriteString(HYP_FORMAT("    HypRegisterPluginModule_{}();\n", mapping.pluginModuleName) + "}\n");
    };

    if (allClasses.Empty())
    {
        // the plugin module's entry point calls it, so it exists even with no classes
        if (mapping.pluginModuleName.Any())
        {
            writer.WriteString(HYP_FORMAT("HYP_EXPORT void {}()\n", GetInitFunctionName(mapping)) + "{\n}\n\n");
        }

        writer.WriteString("} // namespace " + String(BaseNamespace) + "\n");

        writePluginModuleEntry();

        return {};
    }

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
            writer.WriteString(HYP_FORMAT("HYP_EXPORT const Class* g_cls{} = nullptr;", cls.name));
            writer.WriteString(" }\n");
        }
        else
        {
            writer.WriteString(HYP_FORMAT("HYP_EXPORT const Class* g_cls{} = nullptr;\n", cls.name));
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

    // now we need to add a method to be called that initializes all g_clsXXX variables (TClassStaticInit specializations)
    const String initFunctionName = GetInitFunctionName(mapping);
    writer.WriteString(HYP_FORMAT("HYP_EXPORT void {}()\n", initFunctionName) + "{\n");

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

    writePluginModuleEntry();

    return {};
}

Result CXXModuleGenerator::GeneratePluginModuleRegister(const Analyzer& analyzer, const PluginModuleInfo& pluginModule, ByteWriter& writer) const
{
    writer.WriteString(GetGeneratedFilePreamble(String::empty));
    writer.WriteString("#include <Engine/Plugins/PluginModule.hpp>\n\n");

    // compiled into the plugin's library in the editor's engine; with a static engine it's an object of its own, which only a game that uses
    // the plugin links (the archive alone would be dropped by the linker)
    writer.WriteString(HYP_FORMAT("void HypInitializePluginModule_{}();\n\n", pluginModule.name));
    writer.WriteString(HYP_FORMAT("static const bool g_pluginModuleInitializerAdded = ({}::AddPluginModuleInitializer(&HypInitializePluginModule_{}), true);\n",
        BaseNamespace,
        pluginModule.name));

    return {};
}

Result CXXModuleGenerator::GenerateInline(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    const String apiMacro = GetAPIMacroForModule(analyzer, mod);

    // the including .cpp should provide includes for dependencies
    // so we don't add them here
    writer.WriteString("#include <Core/Reflection/ObjectMacros.hpp>\n");
    writer.WriteString("#include <Core/Reflection/ClassUtils.hpp>\n");

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        const bool isComponent = cls.HasAttribute("component");
        const bool isEntity = analyzer.HasBaseClass(cls, "Entity");

        const ClassAttributeValue& structSizeAttributeValue = cls.GetAttribute(Attributes::g_attrSize);
        const ClassAttributeValue& postLoadAttributeValue = cls.GetAttribute(Attributes::g_attrPostLoad);

        // Add minimal feature-specific includes when needed
        if (isComponent || isEntity)
        {
            writer.WriteString("#include <Scene/ComponentInterface.hpp>\n");
        }

        if (isEntity)
        {
            writer.WriteString("#include <Scene/EntityTag.hpp>\n");
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

        // a game module has no ClassDecls.cpp, so it defines its own
        if (analyzer.IsGameModule())
        {
            writer.WriteString(HYP_FORMAT("const Class* g_cls{} = nullptr;\n", cls.name));
        }
        else
        {
            writer.WriteString(HYP_FORMAT("{} extern const Class* g_cls{};\n", apiMacro, cls.name));
        }

        // Implement class StaticClass() to return the static class instance
        // Only for structs, classes:
        if (cls.type == ClassDefinitionType::Struct || cls.type == ClassDefinitionType::Class)
        {
            writer.WriteString("const Class* " + cls.name + "::StaticClass() { " + HYP_FORMAT("return g_cls{};", cls.name) + " }\n");
        }

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

        Set<const ClassDefinition*> baseClassDefinitions;

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
                    if (member.cxxType->IsScriptableDelegate())
                    {
                        // non-const static ScriptableDelegate fields are allowed
                        writer.WriteString(HYP_FORMAT("    StaticField(NAME(HYP_STR({})), &{}::{}", member.friendlyName, cls.name, member.name));
                    }
                    else if (!member.cxxType->isConst && !member.cxxType->isConstexpr)
                    {
                        return HYP_MAKE_ERROR(Error, "Static fields must be const or constexpr");
                    }
                    else
                    {
                        writer.WriteString(HYP_FORMAT("    StaticField(NAME(HYP_STR({})), &{}::{}", member.friendlyName, cls.name, member.name));
                    }
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
                writer.WriteString(HYP_FORMAT("    StaticField(NAME(HYP_STR({})), {}::{}", member.friendlyName, cls.name, member.name));

                if (attributesString.Any())
                {
                    writer.WriteString(", " + attributesString);
                }

                writer.WriteString(")");
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
            // the declared size is the 64-bit one; a 32-bit target (wasm) lays out anything holding a pointer differently
            writer.WriteString(HYP_FORMAT("static_assert(sizeof(void*) != 8 || sizeof({}) == {}, \"Expected sizeof({}) to be {} bytes\");\n", cls.name, structSizeAttributeValue.ToString(), cls.name, structSizeAttributeValue.ToString()));
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
    const String apiMacro = GetAPIMacroForModule(analyzer, mod);

    Set<String> addedIncludes;
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
        FilePath relativePath = analyzer.GetRelativePath(mod.GetPath());

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

                addInclude(analyzer.GetRelativePath(dependencyModule->GetPath()));
            }

            writer.WriteString("\n");
        }
    }

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        const bool isComponent = cls.HasAttribute("component");
        const bool isEntity = analyzer.HasBaseClass(cls, "Entity");

        const ClassAttributeValue& structSizeAttributeValue = cls.GetAttribute(Attributes::g_attrSize);
        const ClassAttributeValue& postLoadAttributeValue = cls.GetAttribute(Attributes::g_attrPostLoad);

        if (isComponent || isEntity)
        {
            addInclude("scene/ComponentInterface.hpp");
        }

        if (isEntity)
        {
            addInclude("scene/EntityTag.hpp");
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

        // a game module has no ClassDecls.cpp, so it defines its own
        if (analyzer.IsGameModule())
        {
            writer.WriteString(HYP_FORMAT("const Class* g_cls{} = nullptr;\n", cls.name));
        }
        else
        {
            writer.WriteString(HYP_FORMAT("{} extern const Class* g_cls{};\n", apiMacro, cls.name));
        }

        // Implement class StaticClass() to return the static class instance
        // Only for structs, classes:
        if (cls.type == ClassDefinitionType::Struct || cls.type == ClassDefinitionType::Class)
        {
            writer.WriteString("const Class* " + cls.name + "::StaticClass() { " + HYP_FORMAT("return g_cls{};", cls.name) + " }\n");
        }

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

        Set<const ClassDefinition*> baseClassDefinitions;

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
                    if (member.cxxType->IsScriptableDelegate())
                    {
                        // non-const static ScriptableDelegate fields are allowed
                        writer.WriteString(HYP_FORMAT("    StaticField(NAME(HYP_STR({})), &{}::{}", member.friendlyName, cls.name, member.name));
                    }
                    else if (!member.cxxType->isConst && !member.cxxType->isConstexpr)
                    {
                        return HYP_MAKE_ERROR(Error, "Static fields must be const or constexpr");
                    }
                    else
                    {
                        writer.WriteString(HYP_FORMAT("    StaticField(NAME(HYP_STR({})), &{}::{}", member.friendlyName, cls.name, member.name));
                    }
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
                writer.WriteString(HYP_FORMAT("    StaticField(NAME(HYP_STR({})), {}::{}", member.friendlyName, cls.name, member.name));

                if (attributesString.Any())
                {
                    writer.WriteString(", " + attributesString);
                }

                writer.WriteString(")");
            }

            if (i != cls.members.Size() - 1)
            {
                writer.WriteString(",");
            }

            writer.WriteString("\n");
        }

        writer.WriteString(HYP_FORMAT("{}\n\n", s_endMacroNames.At(cls.type)));

        writer.WriteString(HYP_FORMAT("#pragma endregion {} Reflection Data\n\n", cls.name));

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
            // the declared size is the 64-bit one; a 32-bit target (wasm) lays out anything holding a pointer differently
            writer.WriteString(HYP_FORMAT("static_assert(sizeof(void*) != 8 || sizeof({}) == {}, \"Expected sizeof({}) to be {} bytes\");\n", cls.name, structSizeAttributeValue.ToString(), cls.name, structSizeAttributeValue.ToString()));
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
