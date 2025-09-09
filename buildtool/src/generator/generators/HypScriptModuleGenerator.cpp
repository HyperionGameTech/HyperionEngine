/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <generator/generators/HypScriptModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <core/Name.hpp>

#include <core/io/ByteWriter.hpp>

namespace hyperion {
namespace buildtool {

FilePath HypScriptModuleGenerator::GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetHypScriptOutputDirectory() / relativePath.BasePath() / StringUtil::StripExtension(relativePath.Basename()) + ".hyp";
}

Result HypScriptModuleGenerator::Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    for (const Pair<String, HypClassDefinition>& pair : mod.GetHypClasses())
    {
        const HypClassDefinition& hypClass = pair.second;

        if (const HypClassAttributeValue& attr = hypClass.GetAttribute("NoScriptBindings"); attr.GetBool())
        {
            continue;
        }

        if (hypClass.type != HypClassDefinitionType::CLASS)
        {
            // only classes are supported for now
            continue;
        }

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

        if (baseClassDefinitions.Size() > 1)
        {
            return HYP_MAKE_ERROR(Error, "Multiple inheritance not supported in HypScript");
        }

        if (baseClassDefinitions.Any())
        {
            writer.WriteString(HYP_FORMAT("extern class {} : {}", hypClass.name, baseClassDefinitions.Front()->name) + " {\n");
        }
        else
        {
            writer.WriteString(HYP_FORMAT("extern class {}", hypClass.name) + " {\n");
        }

        for (SizeType i = 0; i < hypClass.members.Size(); ++i)
        {
            const HypMemberDefinition& member = hypClass.members[i];

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

            /* @TODO: Delegates */
        }

        writer.WriteString("}\n");
    }

    return {};
}

} // namespace buildtool
} // namespace hyperion