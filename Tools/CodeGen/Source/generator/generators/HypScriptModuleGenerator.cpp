/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <generator/generators/HypScriptModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <util/Util.hpp>

#include <Core/Name.hpp>

#include <Core/utilities/DeferredScope.hpp>
#include <Core/utilities/StringUtil.hpp>

#include <Core/logging/Logger.hpp>

#include <Core/io/ByteWriter.hpp>

namespace Hyperion {
namespace CodeGen {

HYP_DECLARE_LOG_CHANNEL(Tool);

static const String s_typeDescriptors[int(ClassDefinitionType::Max)] = {
    "<none>", // ClassDefinitionType::None
    "class",  // ClassDefinitionType::Class
    "struct", // ClassDefinitionType::Struct
    "enum"    // ClassDefinitionType::Enum
};

FilePath HypScriptModuleGenerator::GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetHypScriptOutputDirectory() / relativePath.BasePath() / StringUtil::StripExtension(relativePath.Basename()) + ".hyp";
}

Array<const ClassDefinition*> HypScriptModuleGenerator::SortClassesTopologically(
    const Analyzer& analyzer,
    const Array<const ClassDefinition*>& classes,
    const HashMap<String, SizeType>& classNameToIndex) const
{
    const SizeType numClasses = classes.Size();
    if (numClasses <= 1)
    {
        return classes; // Already sorted or empty
    }

    Array<Array<SizeType>> dependents(numClasses); // dependents[i] contains classes that depend on class i
    Array<SizeType> inDegree(numClasses);          // number of classes this class depends on

    // Initialize inDegree to 0
    for (SizeType i = 0; i < numClasses; i++)
    {
        inDegree[i] = 0;
    }

    // Build dependency graph within this set of classes
    for (SizeType i = 0; i < numClasses; i++)
    {
        const ClassDefinition* currentClass = classes[i];

        for (const String& baseClassName : currentClass->baseClassNames)
        {
            auto it = classNameToIndex.Find(baseClassName);
            if (it != classNameToIndex.End())
            {
                // This class depends on baseClass, so baseClass should come before this class
                const SizeType baseClassIndex = it->second;
                dependents[baseClassIndex].PushBack(i); // baseClass has currentClass as dependent
                inDegree[i]++;                          // currentClass has one more dependency
            }
        }
    }

    Array<SizeType> result;
    Array<SizeType> queue;

    for (SizeType i = 0; i < numClasses; i++)
    {
        if (inDegree[i] == 0)
        {
            queue.PushBack(i);
        }
    }

    while (!queue.Empty())
    {
        const SizeType currentIndex = queue.PopBack();
        result.PushBack(currentIndex);

        for (SizeType dependentIndex : dependents[currentIndex])
        {
            inDegree[dependentIndex]--;
            if (inDegree[dependentIndex] == 0)
            {
                queue.PushBack(dependentIndex);
            }
        }
    }

    if (result.Size() != numClasses)
    {
        for (SizeType i = 0; i < numClasses; i++)
        {
            if (inDegree[i] > 0)
            {
                result.PushBack(i);
            }
        }
    }

    Array<const ClassDefinition*> sortedClasses;
    sortedClasses.Reserve(result.Size());

    for (SizeType index : result)
    {
        sortedClasses.PushBack(classes[index]);
    }

    return sortedClasses;
}

Result HypScriptModuleGenerator::Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    // First, collect all classes that will be generated
    Array<const ClassDefinition*> classesToGenerate;
    HashMap<String, SizeType> classNameToIndex;

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        if (const ClassAttributeValue& attr = cls.GetAttribute("NoScriptBindings"); attr.GetBool())
        {
            continue;
        }

        if (const ClassAttributeValue& attr = cls.GetAttribute("OnlyLanguages"); attr.IsValid() && attr.IsString())
        {
            if (!CheckAttrCSV(attr, "hypscript"))
            {
                continue;
            }
        }

        const SizeType index = classesToGenerate.Size();
        classNameToIndex[cls.name] = index;
        classesToGenerate.PushBack(&cls);
    }

    // Sort classes topologically within this module
    Array<const ClassDefinition*> sortedClasses = SortClassesTopologically(analyzer, classesToGenerate, classNameToIndex);

    // Generate classes in sorted order
    for (const ClassDefinition* cls : sortedClasses)
    {
        if (cls->name == "ObjectBase")
        {
            // Skip emitting ObjectBase class, as it is builtin to the language ("object" is the name in script)
            continue;
        }

        if (cls->type == ClassDefinitionType::Class || cls->type == ClassDefinitionType::Struct)
        {
            HashSet<const ClassDefinition*> baseClassDefinitions;

            if (cls->baseClassNames.Any())
            {
                for (const String& baseClassName : cls->baseClassNames)
                {
                    if (baseClassName == "ObjectBase")
                    {
                        // skip ObjectBase class in base class definitions (it is implied)
                        continue;
                    }

                    const ClassDefinition* baseClassDefinition = analyzer.FindClassDefinition(baseClassName);

                    if (baseClassDefinition)
                    {
                        baseClassDefinitions.Insert(baseClassDefinition);
                    }
                }
            }

            if (baseClassDefinitions.Size() > 1)
            {
                return HYP_MAKE_ERROR(Error, "Multiple inheritance not supported in HypScript");
            }

            if (baseClassDefinitions.Any())
            {
                writer.WriteString(HYP_FORMAT("extern {} {} : {}", s_typeDescriptors[int(cls->type)], cls->name, baseClassDefinitions.Front()->name) + "\n");
            }
            else
            {
                writer.WriteString(HYP_FORMAT("extern {} {}", s_typeDescriptors[int(cls->type)], cls->name) + "\n");
            }

            HYP_DEFER({ writer.WriteString("end\n"); });

            for (SizeType i = 0; i < cls->members.Size(); ++i)
            {
                const MemberDef& member = cls->members[i];

                if (const ClassAttributeValue& attr = member.GetAttribute("NoScriptBindings"); attr.GetBool())
                {
                    // skip generating script bindings for this
                    continue;
                }

                if (const ClassAttributeValue& attr = member.GetAttribute("OnlyLanguages"); attr.IsValid() && attr.IsString())
                {
                    if (!CheckAttrCSV(attr, "hypscript"))
                    {
                        continue;
                    }
                }

                String managedName = member.friendlyName;

                if (const ClassAttributeValue& attr = member.GetAttribute("ManagedName"); attr.IsValid() && attr.IsString())
                {
                    managedName = attr.GetString();
                }

                if (member.type == MemberType::Method)
                {
                    if (!member.cxxType->isFunction)
                    {
                        return HYP_MAKE_ERROR(Error, "Cannot generate script bindings for non-function type");
                    }

                    const ASTFunctionType* functionType = dynamic_cast<const ASTFunctionType*>(member.cxxType.Get());

                    if (!functionType)
                    {
                        return HYP_MAKE_ERROR(Error, "Internal error: failed cast to ASTFunctionType");
                    }

                    HypScriptTypeMapping returnTypeMapping;

                    if (TResult<HypScriptTypeMapping> res = MapToHypScriptType(analyzer, functionType->returnType); res.HasError())
                    {
                        returnTypeMapping = g_hypscriptAnyTypeMapping;
                    }
                    else
                    {
                        returnTypeMapping = res.GetValue();
                    }

                    Array<String> methodArgDecls;
                    Array<String> methodArgNames;

                    for (SizeType i = 0; i < functionType->parameters.Size(); ++i)
                    {
                        const ASTMemberDecl* parameter = functionType->parameters[i];

                        HypScriptTypeMapping parameterTypeMapping;

                        if (TResult<HypScriptTypeMapping> res = MapToHypScriptType(analyzer, parameter->type); res.HasError())
                        {
                            parameterTypeMapping = g_hypscriptAnyTypeMapping;
                        }
                        else
                        {
                            parameterTypeMapping = res.GetValue();
                        }

                        methodArgDecls.PushBack(HYP_FORMAT("{} : {}", parameter->name, parameterTypeMapping.typeName));
                        methodArgNames.PushBack(parameter->name);
                    }

                    writer.WriteString("    ");

                    // static functions
                    if (member.cxxType->isStatic)
                    {
                        writer.WriteString("static ");
                    }

                    writer.WriteString(HYP_FORMAT("{}({}) -> {}\n", managedName, methodArgDecls.Any() ? String::Join(methodArgDecls, ", ") : "", returnTypeMapping.typeName));

                    continue;
                }

                if (member.type == MemberType::Field)
                {
                    HypScriptTypeMapping fieldTypeMapping;

                    if (TResult<HypScriptTypeMapping> res = MapToHypScriptType(analyzer, member.cxxType.Get()); res.HasError())
                    {
                        fieldTypeMapping = g_hypscriptAnyTypeMapping;
                    }
                    else
                    {
                        fieldTypeMapping = res.GetValue();
                    }

                    writer.WriteString("    ");

                    if (member.cxxType->isStatic)
                    {
                        writer.WriteString("static ");
                    }

                    writer.WriteString(HYP_FORMAT("{} : {}\n", managedName, fieldTypeMapping.typeName));

                    continue;
                }

                /* @TODO: Delegates */
            }
        }
        else if (cls->type == ClassDefinitionType::Enum)
        {
            if (cls->baseClassNames.Any()) // underlying type
            {
                if (cls->baseClassNames.Size() > 1)
                {
                    return HYP_MAKE_ERROR(Error, "Enum types may only have one underlying type");
                }

                writer.WriteString(HYP_FORMAT("extern {} {} : {}", s_typeDescriptors[int(ClassDefinitionType::Enum)], cls->name, cls->baseClassNames[0]));
            }
            else
            {
                writer.WriteString(HYP_FORMAT("extern {} {}", s_typeDescriptors[int(ClassDefinitionType::Enum)], cls->name));
            }

            HYP_DEFER({ writer.WriteString("\nend\n"); });

            uint32 enumMemberIndex = 0;

            for (SizeType i = 0; i < cls->members.Size(); ++i)
            {
                const MemberDef& member = cls->members[i];

                if (const ClassAttributeValue& attr = member.GetAttribute("NoScriptBindings"); attr.GetBool())
                {
                    // skip generating script bindings for this
                    continue;
                }

                if (const ClassAttributeValue& attr = member.GetAttribute("OnlyLanguages"); attr.IsValid() && attr.IsString())
                {
                    if (!CheckAttrCSV(attr, "hypscript"))
                    {
                        continue;
                    }
                }

                String managedName = member.friendlyName;

                if (const ClassAttributeValue& attr = member.GetAttribute("ManagedName"); attr.IsValid() && attr.IsString())
                {
                    managedName = attr.GetString();
                }

                if (member.type != MemberType::StaticField)
                {
                    return HYP_MAKE_ERROR(Error, "Only static members allowed in enum types");
                }

                writer.WriteString((enumMemberIndex > 0 ? String(",") : String::empty) + "\n");

                if (member.cxxDecl != nullptr && member.cxxDecl->value != nullptr)
                {
                    writer.WriteString(HYP_FORMAT("    {} = {}", managedName, member.cxxDecl->value->ToString()));
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("    {}", managedName));
                }

                ++enumMemberIndex;
            }
        }
        else
        {
            return HYP_MAKE_ERROR(Error, "Unknown ClassDefinitionType");
        }
    }

    return {};
}

} // namespace CodeGen
} // namespace Hyperion