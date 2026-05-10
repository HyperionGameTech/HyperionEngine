/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/CSharpModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <Core/name/Name.hpp>

#include <Core/utilities/StringUtil.hpp>

#include <Core/io/ByteWriter.hpp>

#include <util/Util.hpp>

namespace Hyperion {
namespace CodeGen {

static const TMap<String, String> s_getValueOverloads = {
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

        for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
        {
            const ClassDefinition& cls = pair.second;

            if (noScriptBindingsPredicate(cls))
            {
                continue;
            }

            if (const ClassAttributeValue& attr = cls.GetAttribute("OnlyLanguages"); attr.IsValid() && attr.IsString())
            {
                if (!CheckAttrCSV(attr, "csharp"))
                {
                    continue;
                }
            }

            if (cls.members.Any())
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

    size_t positionBeforeGlobal = writer.Position();
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

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        size_t positionBeforeLocal = writer.Position();
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

        if (noScriptBindingsPredicate(cls))
        {
            continue;
        }

        if (const ClassAttributeValue& attr = cls.GetAttribute("OnlyLanguages"); attr.IsValid() && attr.IsString())
        {
            if (!CheckAttrCSV(attr, "csharp"))
            {
                continue;
            }
        }

        // skip generation if no members to serialize
        if (cls.members.Empty())
        {
            continue;
        }

        writer.WriteString(HYP_FORMAT("    public static class {}Extensions\n", cls.name));
        writer.WriteString("    {\n");

        for (size_t i = 0; i < cls.members.Size(); ++i)
        {
            const MemberDef& member = cls.members[i];

            if (noScriptBindingsPredicate(member))
            {
                continue;
            }

            if (const ClassAttributeValue& attr = member.GetAttribute("OnlyLanguages"); attr.IsValid() && attr.IsString())
            {
                if (!CheckAttrCSV(attr, "csharp"))
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

                auto MapParameterName = [](const String& paramName) -> String
                {
                    if (paramName == "object"
                        || paramName == "params"
                        || paramName == "event")
                    {
                        return "@" + paramName;
                    }

                    return paramName;
                };

                for (size_t i = 0; i < functionType->parameters.Size(); ++i)
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

                    auto AppendNullable = [](const String& typeName, bool isNullable) -> String
                    {
                        return isNullable ? typeName + "?" : typeName;
                    };

                    const String parameterName = MapParameterName(parameter->name);

                    methodArgDecls.PushBack(HYP_FORMAT("{} {}", AppendNullable(parameterTypeMapping.typeName, parameterTypeMapping.isNullable), parameterName));
                    methodArgNames.PushBack(parameterName);
                }

                writer.WriteString(HYP_FORMAT("        public static {} {}(this {} obj{})\n", AppendNullable(returnTypeMapping.typeName, returnTypeMapping.isNullable), managedName, cls.name, methodArgDecls.Any() ? ", " + String::Join(methodArgDecls, ", ") : ""));
                writer.WriteString("        {\n");

                if (functionType->returnType->IsVoid())
                {
                    if (cls.type == ClassDefinitionType::Struct)
                    {
                        writer.WriteString(HYP_FORMAT("            ObjectBase.GetMethod(Class.GetClass<{}>(), new Name({})).InvokeNative(obj{});\n",
                            cls.name, uint64(CreateStringHashFromDynamicString(member.name.Data())), methodArgNames.Any() ? ", " + String::Join(methodArgNames, ", ") : ""));
                    }
                    else if (cls.type == ClassDefinitionType::Class)
                    {
                        writer.WriteString(HYP_FORMAT("            obj.GetMethod(new Name({})).InvokeNative(obj{});\n",
                            uint64(CreateStringHashFromDynamicString(member.name.Data())), methodArgNames.Any() ? ", " + String::Join(methodArgNames, ", ") : ""));
                    }
                    else
                    {
                        return HYP_MAKE_ERROR(Error, "Unsupported Class type");
                    }
                }
                else
                {
                    if (cls.type == ClassDefinitionType::Struct)
                    {
                        writer.WriteString(HYP_FORMAT("            using (BoxedValueInternal resultData = ObjectBase.GetMethod(Class.GetClass<{}>(), new Name({})).InvokeNative(obj{}))\n",
                            cls.name, uint64(CreateStringHashFromDynamicString(member.name.Data())), methodArgNames.Any() ? ", " + String::Join(methodArgNames, ", ") : ""));
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
                    else if (cls.type == ClassDefinitionType::Class)
                    {
                        writer.WriteString(HYP_FORMAT("            using (BoxedValueInternal resultData = obj.GetMethod(new Name({})).InvokeNative(obj{}))\n",
                            uint64(CreateStringHashFromDynamicString(member.name.Data())), methodArgNames.Any() ? ", " + String::Join(methodArgNames, ", ") : ""));
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
                        return HYP_MAKE_ERROR(Error, "Unsupported Class type");
                    }
                }

                writer.WriteString("        }\n");

                ++numMembersWritten;

                continue;
            }

            // Generate code to get a ScriptableDelegate C# object corresponding to the C++ object
            if (member.type == MemberType::Field && member.cxxType->IsScriptableDelegate())
            {
                writer.WriteString(HYP_FORMAT("        public static ScriptableDelegate Get{}Delegate(this {} obj)\n", managedName, cls.name));
                writer.WriteString("        {\n");

                writer.WriteString(HYP_FORMAT("            Field field = (Field)obj.Class.GetField(new Name({}));\n", uint64(CreateStringHashFromDynamicString(member.friendlyName.Data()))));
                writer.WriteString("            IntPtr fieldAddress = obj.NativeAddress + ((IntPtr)((Field)field).Offset);\n\n");
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

} // namespace CodeGen
} // namespace Hyperion
