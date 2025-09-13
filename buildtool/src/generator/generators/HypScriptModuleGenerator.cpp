/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <generator/generators/HypScriptModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <core/Name.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>

#include <core/io/ByteWriter.hpp>

namespace hyperion {
namespace buildtool {

HYP_DECLARE_LOG_CHANNEL(BuildTool);

FilePath HypScriptModuleGenerator::GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetHypScriptOutputDirectory() / relativePath.BasePath() / StringUtil::StripExtension(relativePath.Basename()) + ".hyp";
}

Array<const HypClassDefinition*> HypScriptModuleGenerator::SortClassesTopologically(const Analyzer& analyzer, const Array<const HypClassDefinition*>& classes, const HashMap<String, SizeType>& classNameToIndex) const
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
        const HypClassDefinition* currentClass = classes[i];

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

    Array<const HypClassDefinition*> sortedClasses;
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
    Array<const HypClassDefinition*> classesToGenerate;
    HashMap<String, SizeType> classNameToIndex;

    for (const Pair<String, HypClassDefinition>& pair : mod.GetHypClasses())
    {
        const HypClassDefinition& hypClass = pair.second;

        if (const HypClassAttributeValue& attr = hypClass.GetAttribute("NoScriptBindings"); attr.GetBool())
        {
            continue;
        }

        const SizeType index = classesToGenerate.Size();
        classNameToIndex[hypClass.name] = index;
        classesToGenerate.PushBack(&hypClass);
    }

    // Sort classes topologically within this module
    Array<const HypClassDefinition*> sortedClasses = SortClassesTopologically(analyzer, classesToGenerate, classNameToIndex);

    // Generate classes in sorted order
    for (const HypClassDefinition* hypClass : sortedClasses)
    {
        if (hypClass->type == HypClassDefinitionType::CLASS || hypClass->type == HypClassDefinitionType::STRUCT)
        {
            HashSet<const HypClassDefinition*> baseClassDefinitions;

            if (hypClass->baseClassNames.Any())
            {
                for (const String& baseClassName : hypClass->baseClassNames)
                {
                    const HypClassDefinition* baseClassDefinition = analyzer.FindHypClassDefinition(baseClassName);

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
                writer.WriteString(HYP_FORMAT("extern class {} : {}", hypClass->name, baseClassDefinitions.Front()->name) + " {\n");
            }
            else
            {
                writer.WriteString(HYP_FORMAT("extern class {}", hypClass->name) + " {\n");
            }

            HYP_DEFER({ writer.WriteString("}\n"); });

            for (SizeType i = 0; i < hypClass->members.Size(); ++i)
            {
                const HypMemberDefinition& member = hypClass->members[i];

                if (const HypClassAttributeValue& attr = member.GetAttribute("NoScriptBindings"); attr.GetBool())
                {
                    // skip generating script bindings for this
                    continue;
                }

                String managedName = member.friendlyName;

                if (const HypClassAttributeValue& attr = member.GetAttribute("ManagedName"); attr.IsValid() && attr.IsString())
                {
                    managedName = attr.GetString();
                }

                if (member.type == HypMemberType::TYPE_METHOD)
                {
                    if (!member.cxxType->isFunction)
                    {
                        return HYP_MAKE_ERROR(Error, "Cannot generate script bindings for non-function type");
                    }

                    if (member.cxxType->isStatic)
                    {
                        continue; // not yet supported
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

                    writer.WriteString(HYP_FORMAT("    {}({}) -> {};\n", managedName, methodArgDecls.Any() ? String::Join(methodArgDecls, ", ") : "", returnTypeMapping.typeName));

                    continue;
                }

                if (member.type == HypMemberType::TYPE_FIELD)
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

                    writer.WriteString(HYP_FORMAT("    {} : {};\n", managedName, fieldTypeMapping.typeName));

                    continue;
                }

                /* @TODO: Delegates, static members */
            }
        }
        else if (hypClass->type == HypClassDefinitionType::ENUM)
        {
            if (hypClass->baseClassNames.Any()) // underlying type
            {
                if (hypClass->baseClassNames.Size() > 1)
                {
                    return HYP_MAKE_ERROR(Error, "Enum types may only have one underlying type");
                }

                writer.WriteString(HYP_FORMAT("extern enum {} : {}", hypClass->name, hypClass->baseClassNames[0]) + " {");
            }
            else
            {
                writer.WriteString(HYP_FORMAT("extern enum {}", hypClass->name) + " {");
            }

            HYP_DEFER({ writer.WriteString("\n}\n"); });

            uint32 enumMemberIndex = 0;

            for (SizeType i = 0; i < hypClass->members.Size(); ++i)
            {
                const HypMemberDefinition& member = hypClass->members[i];

                if (const HypClassAttributeValue& attr = member.GetAttribute("NoScriptBindings"); attr.GetBool())
                {
                    // skip generating script bindings for this
                    continue;
                }

                String managedName = member.friendlyName;

                if (const HypClassAttributeValue& attr = member.GetAttribute("ManagedName"); attr.IsValid() && attr.IsString())
                {
                    managedName = attr.GetString();
                }

                if (member.type != HypMemberType::TYPE_CONSTANT)
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
            return HYP_MAKE_ERROR(Error, "Unknown HypClassDefinitionType");
        }
    }

    return {};
}

} // namespace buildtool
} // namespace hyperion