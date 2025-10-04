/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <generator/generators/CSharpModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <core/Name.hpp>

#include <core/io/ByteWriter.hpp>

#include <util/ParseUtil.hpp>

#include <time.h>

namespace hyperion {
namespace buildtool {

static const HashMap<String, String> g_getvalueOverloads = {
    { "bool", "ReadBool" },
    { "sbyte", "ReadInt8" },
    { "byte", "ReadUInt8" },
    { "short", "ReadInt16" },
    { "ushort", "ReadUInt16" },
    { "int", "ReadInt32" },
    { "uint", "ReadUInt32" },
    { "long", "ReadInt64" },
    { "ulong", "ReadUInt64" },
    { "float", "ReadFloat" },
    { "double", "ReadDouble" },
    { "string", "ReadString" },
    { "Name", "ReadName" },
    { "byte[]", "ReadByteBuffer" },
    { "ObjIdBase", "ReadId" }
};

FilePath CSharpModuleGenerator::GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetCSharpOutputDirectory() / relativePath.BasePath() / StringUtil::StripExtension(relativePath.Basename()) + ".cs";
}

Result CSharpModuleGenerator::Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    static const auto noScriptBindingsPredicate = []<class T>(const T& item)
    {
        return item.GetAttribute("NoScriptBindings").GetBool();
    };

    static const auto noScriptBindingsPredicateInv = []<class T>(const T& item)
    {
        return !item.GetAttribute("NoScriptBindings").GetBool();
    };

    {
        // check if all classes are empty; if so, skip generation entirely
        bool allEmpty = true;

        for (const Pair<String, HypClassDefinition>& pair : mod.GetHypClasses())
        {
            const HypClassDefinition& hypClass = pair.second;

            if (noScriptBindingsPredicate(hypClass))
            {
                continue;
            }

            if (hypClass.members.Any())
            {
                allEmpty = false;
                break;
            }
        }

        if (allEmpty)
        {
            return Result();
        }
    }

    SizeType positionBeforeGlobal = writer.Position();
    int numClassesWritten = 0;

    HYP_DEFER({
        if (numClassesWritten == 0)
        {
            writer.Seek(positionBeforeGlobal, /* truncate */ true);
        }
    });

    writer.WriteString(GetGeneratedFilePreamble(FilePath::Relative(mod.GetPath(), analyzer.GetSourceDirectory())));

    writer.WriteString("namespace Hyperion\n");
    writer.WriteString("{\n");

    for (const Pair<String, HypClassDefinition>& pair : mod.GetHypClasses())
    {
        const HypClassDefinition& hypClass = pair.second;

        SizeType positionBeforeLocal = writer.Position();
        int numMembersWritten = 0;

        HYP_DEFER({
            if (numMembersWritten == 0)
            {
                writer.Seek(positionBeforeLocal, /* truncate */ true);
            }
            else
            {
                ++numClassesWritten;
            }
        });

        if (noScriptBindingsPredicate(hypClass))
        {
            continue;
        }

        // skip generation if no members to serialize
        if (hypClass.members.Empty())
        {
            continue;
        }

        writer.WriteString(HYP_FORMAT("    public static class {}Extensions\n", hypClass.name));
        writer.WriteString("    {\n");

        for (SizeType i = 0; i < hypClass.members.Size(); ++i)
        {
            const HypMemberDefinition& member = hypClass.members[i];

            if (noScriptBindingsPredicate(member))
            {
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
                    continue;
                }

                const ASTFunctionType* functionType = dynamic_cast<const ASTFunctionType*>(member.cxxType.Get());

                if (!functionType)
                {
                    return HYP_MAKE_ERROR(Error, "Internal error: failed cast to ASTFunctionType");
                }

                CSharpTypeMapping returnTypeMapping;

                if (TResult<CSharpTypeMapping> res = MapToCSharpType(analyzer, functionType->returnType); res.HasError())
                {
                    return res.GetError();
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

                    CSharpTypeMapping parameterTypeMapping;

                    if (TResult<CSharpTypeMapping> res = MapToCSharpType(analyzer, parameter->type); res.HasError())
                    {
                        return res.GetError();
                    }
                    else
                    {
                        parameterTypeMapping = res.GetValue();
                    }

                    methodArgDecls.PushBack(HYP_FORMAT("{} {}", parameterTypeMapping.typeName, parameter->name));
                    methodArgNames.PushBack(parameter->name);
                }

                writer.WriteString(HYP_FORMAT("        public static {} {}(this {} obj{})\n", returnTypeMapping.typeName, managedName, hypClass.name, methodArgDecls.Any() ? ", " + String::Join(methodArgDecls, ", ") : ""));
                writer.WriteString("        {\n");

                if (functionType->returnType->IsVoid())
                {
                    if (hypClass.type == HypClassDefinitionType::STRUCT)
                    {
                        writer.WriteString(HYP_FORMAT("            HypObject.GetMethod(HypClass.GetClass<{}>(), new Name({})).InvokeNative(obj{});\n",
                            hypClass.name, uint64(CreateWeakNameFromDynamicString(member.name.Data())), methodArgNames.Any() ? ", " + String::Join(methodArgNames, ", ") : ""));
                    }
                    else if (hypClass.type == HypClassDefinitionType::CLASS)
                    {
                        writer.WriteString(HYP_FORMAT("            obj.GetMethod(new Name({})).InvokeNative(obj{});\n",
                            uint64(CreateWeakNameFromDynamicString(member.name.Data())), methodArgNames.Any() ? ", " + String::Join(methodArgNames, ", ") : ""));
                    }
                    else
                    {
                        return HYP_MAKE_ERROR(Error, "Unsupported HypClass type");
                    }
                }
                else
                {
                    if (hypClass.type == HypClassDefinitionType::STRUCT)
                    {
                        writer.WriteString(HYP_FORMAT("            using (HypDataBuffer resultData = HypObject.GetMethod(HypClass.GetClass<{}>(), new Name({})).InvokeNative(obj{}))\n",
                            hypClass.name, uint64(CreateWeakNameFromDynamicString(member.name.Data())), methodArgNames.Any() ? ", " + String::Join(methodArgNames, ", ") : ""));
                        writer.WriteString("            {\n");

                        if (returnTypeMapping.getValueOverload.HasValue())
                        {
                            writer.WriteString(HYP_FORMAT("                return resultData.{}();\n", returnTypeMapping.getValueOverload.Get()));
                        }
                        else
                        {
                            writer.WriteString(HYP_FORMAT("                return ({})resultData.GetValue();\n", returnTypeMapping.typeName));
                        }

                        writer.WriteString("            }\n");
                    }
                    else if (hypClass.type == HypClassDefinitionType::CLASS)
                    {
                        writer.WriteString(HYP_FORMAT("            using (HypDataBuffer resultData = obj.GetMethod(new Name({})).InvokeNative(obj{}))\n",
                            uint64(CreateWeakNameFromDynamicString(member.name.Data())), methodArgNames.Any() ? ", " + String::Join(methodArgNames, ", ") : ""));
                        writer.WriteString("            {\n");

                        if (returnTypeMapping.getValueOverload.HasValue())
                        {
                            writer.WriteString(HYP_FORMAT("                return resultData.{}();\n", returnTypeMapping.getValueOverload.Get()));
                        }
                        else
                        {
                            writer.WriteString(HYP_FORMAT("                return ({})resultData.GetValue();\n", returnTypeMapping.typeName));
                        }

                        writer.WriteString("            }\n");
                    }
                    else
                    {
                        return HYP_MAKE_ERROR(Error, "Unsupported HypClass type");
                    }
                }

                writer.WriteString("        }\n");

                ++numMembersWritten;

                continue;
            }

            // Generate code to get a ScriptableDelegate C# object corresponding to the C++ object
            if (member.type == HypMemberType::TYPE_FIELD && member.cxxType->IsScriptableDelegate())
            {
                writer.WriteString(HYP_FORMAT("        public static ScriptableDelegate Get{}Delegate(this {} obj)\n", managedName, hypClass.name));
                writer.WriteString("        {\n");

                writer.WriteString(HYP_FORMAT("            HypField field = (HypField)obj.HypClass.GetField(new Name({}));\n", uint64(CreateWeakNameFromDynamicString(member.friendlyName.Data()))));
                writer.WriteString("            IntPtr fieldAddress = obj.NativeAddress + ((IntPtr)((HypField)field).Offset);\n\n");
                writer.WriteString("            return new ScriptableDelegate(obj, fieldAddress);\n");

                writer.WriteString("        }\n");

                ++numMembersWritten;

                continue;
            }
        }

        writer.WriteString("    }\n");
    }

    writer.WriteString("}\n");

    return {};
}

} // namespace buildtool
} // namespace hyperion