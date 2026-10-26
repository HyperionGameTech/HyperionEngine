/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/CSharpModuleGenerator.hpp>
#include <generator/generators/BindingsShared.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <util/Util.hpp>

namespace Hyperion {
namespace CodeGen {

namespace {

struct GameProperty
{
    String name;
    CSharpTypeMapping mapping;

    bool canGet : 1 = false;
    bool canSet : 1 = false;
};

const Map<String, String> g_getValueOverloads = {
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

String AppendNullable(const String& typeName, bool isNullable)
{
    return isNullable ? typeName + "?" : typeName;
}

constexpr UTF8StringView InteropServices = "global::System.Runtime.InteropServices";

String NativeLibraryForModule(const Analyzer& analyzer, const Module& mod)
{
    const String relativePath = String(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str()).ReplaceAll("\\", "/");

    if (relativePath.StartsWith("Core/"))
    {
        return "hyperion-core";
    }

    if (relativePath.StartsWith("Engine/Net/"))
    {
        return "hyperion-net";
    }

    return "hyperion";
}

String EnumUnderlyingCSharpType(const Analyzer& analyzer, const String& enumName)
{
    static const Map<String, String> s_underlyingTypes {
        { "int", "int" },
        { "uint8", "byte" },
        { "uint16", "ushort" },
        { "uint32", "uint" },
        { "uint64", "ulong" },
        { "int8", "sbyte" },
        { "int16", "short" },
        { "int32", "int" },
        { "int64", "long" }
    };

    const ClassDefinition* enumDefinition = analyzer.FindClassDefinition(enumName);

    if (!enumDefinition || enumDefinition->type != ClassDefinitionType::Enum || enumDefinition->baseClassNames.Size() > 1)
    {
        return String::empty;
    }

    const String cxxUnderlying = enumDefinition->baseClassNames.Any() ? enumDefinition->baseClassNames.Front() : String("int");

    auto it = s_underlyingTypes.Find(cxxUnderlying);

    return it != s_underlyingTypes.End() ? it->second : String::empty;
}

String ArrayElementCSharpType(const StrataTypeMapping& mapping)
{
    static const Map<String, String> s_elementTypes {
        { "float2", "Vec2f" },
        { "float3", "Vec3f" },
        { "float4", "Vec4f" }
    };

    auto it = s_elementTypes.Find(mapping.elementTypeName);

    return it != s_elementTypes.End() ? it->second : mapping.elementTypeName;
}

bool TryEmitDirectBinding(
    const Analyzer& analyzer,
    const ClassDefinition& cls,
    const BindingsShared::MethodThunk& thunk,
    const String& managedName,
    const String& libraryName,
    const CSharpTypeMapping& returnTypeMapping,
    const Array<CSharpTypeMapping>& paramTypeMappings,
    const Array<String>& methodArgDecls,
    const Array<String>& methodArgNames,
    String& out)
{
    static const Set<String> s_scalarTypes {
        "int", "uint", "float", "double", "long", "ulong", "short", "ushort", "byte", "sbyte", "IntPtr", "bool"
    };

    static const String s_bindings = "global::Hyperion.NativeBindings";
    static const String s_abiArray = "global::Hyperion.ABIArray";

    if (!libraryName.Any() || thunk.isStatic)
    {
        return false;
    }

    if (cls.type != ClassDefinitionType::Class && cls.type != ClassDefinitionType::Struct)
    {
        return false;
    }

    Array<String> nativeParams;
    Array<String> nativeArgs;
    Array<String> prelude;
    Array<String> fixedStatements;
    Array<String> argDecls = methodArgDecls;
    bool usesStrings = false;

    if (cls.type == ClassDefinitionType::Class)
    {
        nativeParams.PushBack("nint self");
        nativeArgs.PushBack("obj.NativeAddress");
    }
    else if (BindingsShared::IsBlittableStruct(analyzer, cls.name))
    {
        nativeParams.PushBack(HYP_FORMAT("{}* self", cls.name));
        nativeArgs.PushBack("&obj");
    }
    else
    {
        return false;
    }

    for (size_t i = 0; i < thunk.paramMappings.Size(); i++)
    {
        const StrataTypeMapping& strataMapping = thunk.paramMappings[i];
        const CSharpTypeMapping& csharpMapping = paramTypeMappings[i];
        const String& name = methodArgNames[i];
        const String localName = name.StartsWith("@") ? String(name.Substr(1)) : name;

        if (strataMapping.isArray)
        {
            if (strataMapping.isStringArray || csharpMapping.typeName != "Array")
            {
                return false;
            }

            const String elementType = strataMapping.isHandleArray ? String("nint") : ArrayElementCSharpType(strataMapping);

            prelude.PushBack(strataMapping.isHandleArray
                                 ? HYP_FORMAT("nint[] {}Elements = {}.ToNativeAddresses({});", localName, s_bindings, name)
                                 : HYP_FORMAT("{}[] {}Elements = {}.ToTypedArray<{}>({});", elementType, localName, s_bindings, elementType, name));

            fixedStatements.PushBack(HYP_FORMAT("fixed ({}* {}Data = {}Elements)", elementType, localName, localName));

            nativeParams.PushBack(HYP_FORMAT("{}* {}", s_abiArray, name));
            nativeArgs.PushBack(HYP_FORMAT("&{}Array", localName));
        }
        else if (strataMapping.isString)
        {
            if (csharpMapping.typeName != "string")
            {
                return false;
            }

            nativeParams.PushBack(HYP_FORMAT("string {}", name));
            nativeArgs.PushBack(name);
            usesStrings = true;
        }
        else if (strataMapping.isHandle)
        {
            if (csharpMapping.typeName == "Object" || csharpMapping.typeName == "object")
            {
                return false;
            }

            nativeParams.PushBack(HYP_FORMAT("nint {}", name));
            nativeArgs.PushBack(HYP_FORMAT("({} == null ? 0 : {}.NativeAddress)", name, name));
        }
        else if (strataMapping.isEnum)
        {
            const String underlying = EnumUnderlyingCSharpType(analyzer, strataMapping.typeName);

            if (!underlying.Any())
            {
                return false;
            }

            nativeParams.PushBack(HYP_FORMAT("{} {}", underlying, name));
            nativeArgs.PushBack(HYP_FORMAT("({}){}", underlying, name));
        }
        else if (strataMapping.isVector || strataMapping.isStructValue)
        {
            if (csharpMapping.isNullable
                || (strataMapping.isStructValue && (csharpMapping.typeName != strataMapping.typeName || !BindingsShared::IsBlittableStruct(analyzer, strataMapping.typeName))))
            {
                return false;
            }

            nativeParams.PushBack(HYP_FORMAT("{}* {}", csharpMapping.typeName, name));

            // C++ `T&` writes back, so the caller's variable is passed by ref and pinned
            if (BindingsShared::IsMutableReference(thunk.functionType->parameters[i]->type.Get()))
            {
                argDecls[i] = HYP_FORMAT("ref {} {}", csharpMapping.typeName, name);
                fixedStatements.PushBack(HYP_FORMAT("fixed ({}* {}Ptr = &{})", csharpMapping.typeName, localName, name));
                nativeArgs.PushBack(HYP_FORMAT("{}Ptr", localName));
            }
            else
            {
                nativeArgs.PushBack(HYP_FORMAT("&{}", name));
            }
        }
        else if (s_scalarTypes.Contains(csharpMapping.typeName))
        {
            nativeParams.PushBack(csharpMapping.typeName == "bool"
                                      ? HYP_FORMAT("[{}.MarshalAs({}.UnmanagedType.U1)] bool {}", InteropServices, InteropServices, name)
                                      : HYP_FORMAT("{} {}", csharpMapping.typeName, name));
            nativeArgs.PushBack(name);
        }
        else
        {
            return false;
        }
    }

    const StrataTypeMapping& strataReturn = thunk.returnMapping;
    const ASTFunctionType* functionType = thunk.functionType;

    String nativeReturnType = "void";
    bool returnsBool = false;
    Array<String> body;

    auto nativeCall = [&]()
    {
        return HYP_FORMAT("Native{}({})", managedName, String::Join(nativeArgs, ", "));
    };

    if (thunk.propertySetterIdiom)
    {
        if (returnTypeMapping.typeName != cls.name)
        {
            return false;
        }

        body.PushBack(nativeCall() + ";");
        body.PushBack("return obj;");
    }
    else if (functionType->returnType->IsVoid())
    {
        body.PushBack(nativeCall() + ";");
    }
    else if (strataReturn.isArray)
    {
        if (returnTypeMapping.typeName != "Array")
        {
            return false;
        }

        nativeParams.PushBack(HYP_FORMAT("{}* outReturn", s_abiArray));
        nativeArgs.PushBack("&result");
        body.PushBack(HYP_FORMAT("{} result = default;", s_abiArray));
        body.PushBack(nativeCall() + ";");

        if (strataReturn.isHandleArray)
        {
            body.PushBack(HYP_FORMAT("return {}.TakeObjectArray<{}>(&result);", s_bindings, strataReturn.elementTypeName));
        }
        else if (strataReturn.isStringArray)
        {
            body.PushBack(HYP_FORMAT("return {}.TakeStringArray(&result);", s_bindings));
        }
        else
        {
            body.PushBack(HYP_FORMAT("return {}.TakeArray<{}>(&result);", s_bindings, ArrayElementCSharpType(strataReturn)));
        }
    }
    else if (strataReturn.isString)
    {
        if (returnTypeMapping.typeName != "string")
        {
            return false;
        }

        nativeParams.PushBack("global::Hyperion.ABIString* outReturn");
        nativeArgs.PushBack("&result");
        body.PushBack("global::Hyperion.ABIString result = default;");
        body.PushBack(nativeCall() + ";");
        body.PushBack(HYP_FORMAT("return {}.TakeString(&result);", s_bindings));
    }
    else if (strataReturn.isVector || strataReturn.isStructValue)
    {
        if (returnTypeMapping.isNullable
            || (strataReturn.isStructValue && (returnTypeMapping.typeName != strataReturn.typeName || !BindingsShared::IsBlittableStruct(analyzer, strataReturn.typeName))))
        {
            return false;
        }

        nativeParams.PushBack(HYP_FORMAT("{}* outReturn", returnTypeMapping.typeName));
        nativeArgs.PushBack("&result");
        body.PushBack(HYP_FORMAT("{} result = default;", returnTypeMapping.typeName));
        body.PushBack(nativeCall() + ";");
        body.PushBack("return result;");
    }
    else if (strataReturn.isHandle)
    {
        if (returnTypeMapping.typeName == "Object" || returnTypeMapping.typeName == "object")
        {
            return false;
        }

        nativeReturnType = "nint";
        body.PushBack(HYP_FORMAT("return {}.{}<{}>({});", s_bindings,
                                 strataReturn.isStrongHandle ? "TakeManagedObject" : "GetManagedObject", returnTypeMapping.typeName, nativeCall()));
    }
    else if (strataReturn.isEnum)
    {
        const String underlying = EnumUnderlyingCSharpType(analyzer, strataReturn.typeName);

        if (!underlying.Any())
        {
            return false;
        }

        nativeReturnType = underlying;
        body.PushBack(HYP_FORMAT("return ({}){};", returnTypeMapping.typeName, nativeCall()));
    }
    else if (s_scalarTypes.Contains(returnTypeMapping.typeName))
    {
        nativeReturnType = returnTypeMapping.typeName;
        returnsBool = returnTypeMapping.typeName == "bool";
        body.PushBack(HYP_FORMAT("return {};", nativeCall()));
    }
    else
    {
        return false;
    }

    out += HYP_FORMAT("        [{}.LibraryImport(\"{}\", EntryPoint = \"{}\"{})]\n",
                      InteropServices, libraryName, thunk.symbol,
                      usesStrings ? HYP_FORMAT(", StringMarshalling = {}.StringMarshalling.Utf8", InteropServices) : String::empty);

    if (returnsBool)
    {
        out += HYP_FORMAT("        [return: {}.MarshalAs({}.UnmanagedType.U1)]\n", InteropServices, InteropServices);
    }

    out += HYP_FORMAT("        private static partial {} Native{}({});\n\n", nativeReturnType, managedName, String::Join(nativeParams, ", "));

    out += HYP_FORMAT("        public static {} {}(this {} obj{})\n",
                      AppendNullable(returnTypeMapping.typeName, returnTypeMapping.isNullable), managedName, cls.name,
                      argDecls.Any() ? ", " + String::Join(argDecls, ", ") : "");
    out += "        {\n";

    String indent = "            ";

    for (const String& line : prelude)
    {
        out += indent + line + "\n";
    }

    if (fixedStatements.Any())
    {
        for (const String& statement : fixedStatements)
        {
            out += indent + statement + "\n";
        }

        out += indent + "{\n";
        indent += "    ";

        for (size_t i = 0; i < thunk.paramMappings.Size(); i++)
        {
            if (!thunk.paramMappings[i].isArray)
            {
                continue;
            }

            const String& name = methodArgNames[i];
            const String localName = name.StartsWith("@") ? String(name.Substr(1)) : name;

            out += indent + HYP_FORMAT("{} {}Array = new {}({}Data, {}Elements.Length);\n", s_abiArray, localName, s_abiArray, localName, localName);
        }
    }

    for (const String& line : body)
    {
        out += indent + line + "\n";
    }

    if (fixedStatements.Any())
    {
        out += "            }\n";
    }

    out += "        }\n";

    return true;
}

bool IsBoundInCSharp(const Analyzer& analyzer, const ClassDefinition& cls)
{
    if (analyzer.HasAttrInHierarchy(cls, Attributes::g_attrNoScriptBindings))
    {
        return false;
    }

    if (const ClassAttributeValue& attr = cls.GetAttribute(Attributes::g_attrOnlyLanguages); attr.IsValid() && attr.IsString())
    {
        return CheckAttrCSV(attr, "csharp");
    }

    return true;
}

bool HasCSharpClass(const Analyzer& analyzer, const ClassDefinition& cls)
{
    if (cls.declModule != nullptr)
    {
        return cls.type == ClassDefinitionType::Class && IsBoundInCSharp(analyzer, cls);
    }

    return cls.hasCSharpClass || analyzer.GetBuiltinClasses().Contains(cls.name);
}

String QualifiedCSharpName(const ClassDefinition& cls)
{
    return cls.namespaceParts.Any()
        ? HYP_FORMAT("global::{}.{}", String::Join(cls.namespaceParts, '.'), cls.name)
        : HYP_FORMAT("global::{}", cls.name);
}

String CSharpBaseClassName(const Analyzer& analyzer, const ClassDefinition& cls)
{
    const ClassDefinition* current = &cls;

    while (true)
    {
        const ClassDefinition* base = nullptr;

        for (const String& baseClassName : current->baseClassNames)
        {
            if ((base = analyzer.FindClassDefinition(baseClassName)) != nullptr)
            {
                break;
            }
        }

        if (base == nullptr)
        {
            return "global::Hyperion.ObjectBase";
        }

        if (HasCSharpClass(analyzer, *base))
        {
            return QualifiedCSharpName(*base);
        }

        current = base;
    }
}

void AddGameProperty(Array<GameProperty>& properties, const String& name, const CSharpTypeMapping& mapping, bool canGet, bool canSet)
{
    for (GameProperty& property : properties)
    {
        if (property.name == name)
        {
            property.canGet |= canGet;
            property.canSet |= canSet;

            return;
        }
    }

    properties.PushBack(GameProperty { name, mapping, canGet, canSet });
}

// HYP_FIELDs and Property= getters/setters become C# properties that go through the native Property of the same name
Array<GameProperty> CollectGameProperties(const Analyzer& analyzer, const ClassDefinition& cls)
{
    Array<GameProperty> properties;

    for (const MemberDef& member : cls.members)
    {
        if (member.GetAttribute(Attributes::g_attrNoScriptBindings).GetBool() || !member.cxxType)
        {
            continue;
        }

        const ClassAttributeValue& propertyAttribute = member.GetAttribute(Attributes::g_attrProperty);

        if (!propertyAttribute.IsString())
        {
            continue;
        }

        const String propertyName = String(propertyAttribute.GetString());

        // a C# member can't share its class's name
        if (propertyName == cls.name)
        {
            continue;
        }

        if (member.type == MemberType::Field)
        {
            if (member.cxxType->isStatic || member.cxxType->IsScriptableDelegate())
            {
                continue;
            }

            if (TResult<CSharpTypeMapping> res = MapToCSharpType(analyzer, member.cxxType.Get()); res.HasValue() && res.GetValue().typeName != "Array")
            {
                AddGameProperty(properties, propertyName, res.GetValue(), true, !member.cxxType->isConst);
            }

            continue;
        }

        if (member.type != MemberType::Method || !member.cxxType->isFunction || member.cxxType->isStatic)
        {
            continue;
        }

        const ASTFunctionType* functionType = dynamic_cast<const ASTFunctionType*>(member.cxxType.Get());

        if (functionType == nullptr)
        {
            continue;
        }

        if (functionType->parameters.Empty() && !functionType->returnType->IsVoid())
        {
            if (TResult<CSharpTypeMapping> res = MapToCSharpType(analyzer, functionType->returnType); res.HasValue() && res.GetValue().typeName != "Array")
            {
                AddGameProperty(properties, propertyName, res.GetValue(), true, false);
            }
        }
        else if (functionType->parameters.Size() == 1)
        {
            if (TResult<CSharpTypeMapping> res = MapToCSharpType(analyzer, functionType->parameters[0]->type); res.HasValue() && res.GetValue().typeName != "Array")
            {
                AddGameProperty(properties, propertyName, res.GetValue(), false, true);
            }
        }
    }

    return properties;
}

void EmitGameProperty(const GameProperty& property, ByteWriter& writer)
{
    const uint64 nameHash = uint64(CreateStringHashFromDynamicString(property.name.Data()));
    const String typeName = AppendNullable(property.mapping.typeName, property.mapping.isNullable);

    writer.WriteString(HYP_FORMAT("        public {} {}\n", typeName, property.name));
    writer.WriteString("        {\n");

    if (property.canGet)
    {
        writer.WriteString("            get\n");
        writer.WriteString("            {\n");
        writer.WriteString(HYP_FORMAT("                using (BoxedValue value = this.Class.GetProperty(new Name({}))!.Value.Get(this))\n", nameHash));
        writer.WriteString("                {\n");

        if (property.mapping.getValueOverload.HasValue())
        {
            writer.WriteString(HYP_FORMAT("                    return value.Buffer.{}();\n", property.mapping.getValueOverload.Get()));
        }
        else
        {
            writer.WriteString(HYP_FORMAT("                    return ({})value.GetValue()!;\n", typeName));
        }

        writer.WriteString("                }\n");
        writer.WriteString("            }\n");
    }

    if (property.canSet)
    {
        writer.WriteString("            set\n");
        writer.WriteString("            {\n");
        writer.WriteString("                using (BoxedValue boxed = new BoxedValue(value))\n");
        writer.WriteString("                {\n");
        writer.WriteString(HYP_FORMAT("                    this.Class.GetProperty(new Name({}))!.Value.Set(this, boxed);\n", nameHash));
        writer.WriteString("                }\n");
        writer.WriteString("            }\n");
    }

    writer.WriteString("        }\n");
}

// a game module's types have no hand-written C# counterpart, so the declarations are generated along with the extension methods
int EmitGameTypes(const Analyzer& analyzer, const Module& mod, ByteWriter& writer)
{
    int numTypesWritten = 0;

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        if (!IsBoundInCSharp(analyzer, cls))
        {
            continue;
        }

        if (cls.condition.Any())
        {
            continue;
        }

        if (cls.type == ClassDefinitionType::Class)
        {
            writer.WriteString(HYP_FORMAT("    [ClassBinding(Name = \"{}\")]\n", cls.name));
            writer.WriteString(HYP_FORMAT("    public partial class {} : {}\n", cls.name, CSharpBaseClassName(analyzer, cls)));
            writer.WriteString("    {\n");

            const Array<GameProperty> properties = CollectGameProperties(analyzer, cls);

            for (size_t i = 0; i < properties.Size(); i++)
            {
                if (i != 0)
                {
                    writer.WriteString("\n");
                }

                EmitGameProperty(properties[i], writer);
            }

            writer.WriteString("    }\n\n");

            ++numTypesWritten;
        }
        else if (cls.type == ClassDefinitionType::Enum)
        {
            String underlyingType = EnumUnderlyingCSharpType(analyzer, cls.name);

            if (underlyingType.Empty())
            {
                underlyingType = "int";
            }

            writer.WriteString(HYP_FORMAT("    [ClassBinding(Name = \"{}\")]\n", cls.name));
            writer.WriteString(HYP_FORMAT("    public enum {} : {}\n", cls.name, underlyingType));
            writer.WriteString("    {\n");

            for (const MemberDef& member : cls.members)
            {
                if (member.cxxDecl && member.cxxDecl->value)
                {
                    writer.WriteString(HYP_FORMAT("        {} = {},\n", member.name, member.cxxDecl->value->ToString()));
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("        {},\n", member.name));
                }
            }

            writer.WriteString("    }\n\n");

            ++numTypesWritten;
        }
    }

    return numTypesWritten;
}

} // namespace

FilePath CSharpModuleGenerator::GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetCSharpOutputDirectory() / relativePath.BasePath() / String(StringUtil::StripExtension(relativePath.Basename())) + ".cs";
}

Result CSharpModuleGenerator::Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    static const auto noScriptBindingsPredicate = []<class T>(const T& item)
    {
        return item.GetAttribute(Attributes::g_attrNoScriptBindings).GetBool();
    };

    static const auto noScriptBindingsPredicateInv = []<class T>(const T& item)
    {
        return !item.GetAttribute(Attributes::g_attrNoScriptBindings).GetBool();
    };

    if (!analyzer.IsGameModule())
    {
        // check if all classes are empty; if so, skip generation entirely
        bool allEmpty = true;

        for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
        {
            const ClassDefinition& cls = pair.second;

            if (analyzer.HasAttrInHierarchy(cls, Attributes::g_attrNoScriptBindings))
            {
                continue;
            }

            if (const ClassAttributeValue& attr = cls.GetAttribute(Attributes::g_attrOnlyLanguages); attr.IsValid() && attr.IsString())
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

    static const Set<String> allHandleNames = BindingsShared::CollectDeclaredTypeNames(analyzer);

    // the editor loads a renamed copy of a game module, so a LibraryImport of it would bind to the wrong file
    const String libraryName = mod.GetPath().Any() && !analyzer.IsGameModule() ? NativeLibraryForModule(analyzer, mod) : String::empty;

    String namespaceName;

    if (analyzer.IsGameModule())
    {
        for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
        {
            const String classNamespace = pair.second.namespaceParts.Any() ? String::Join(pair.second.namespaceParts, '.') : String(BaseNamespace);

            if (namespaceName.Empty())
            {
                namespaceName = classNamespace;
            }
            else if (classNamespace != namespaceName)
            {
                return HYP_MAKE_ERROR(Error, "C# bindings need all the types in {} to share one namespace", mod.GetPath());
            }
        }
    }

    if (namespaceName.Empty())
    {
        namespaceName = BaseNamespace;
    }

    writer.WriteString(GetGeneratedFilePreamble(FilePath::Relative(mod.GetPath(), analyzer.GetSourceDirectory())));

    if (analyzer.IsGameModule())
    {
        writer.WriteString(HYP_FORMAT("using {};\n\n", BaseNamespace));
    }

    writer.WriteString(HYP_FORMAT("namespace {}\n", namespaceName));
    writer.WriteString("{\n");

    if (analyzer.IsGameModule())
    {
        numClassesWritten += EmitGameTypes(analyzer, mod, writer);
    }

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

                Mutex::Guard guard(m_writtenClassNamesMutex);
                m_writtenClassNames.Insert(cls.name);
            }
        });

        if (analyzer.HasAttrInHierarchy(cls, Attributes::g_attrNoScriptBindings))
        {
            continue;
        }

        if (const ClassAttributeValue& attr = cls.GetAttribute(Attributes::g_attrOnlyLanguages); attr.IsValid() && attr.IsString())
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

        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#if {}\n", cls.condition));
        }

        writer.WriteString(HYP_FORMAT("    public static unsafe partial class {}Extensions\n", cls.name));
        writer.WriteString("    {\n");

        const Array<BindingsShared::MethodThunk> thunks = BindingsShared::CollectMethodThunks(analyzer, cls, allHandleNames);

        for (size_t i = 0; i < cls.members.Size(); ++i)
        {
            const MemberDef& member = cls.members[i];

            if (noScriptBindingsPredicate(member))
            {
                continue;
            }

            if (const ClassAttributeValue& attr = member.GetAttribute(Attributes::g_attrOnlyLanguages); attr.IsValid() && attr.IsString())
            {
                if (!CheckAttrCSV(attr, "csharp"))
                {
                    continue;
                }
            }

            const size_t positionBeforeMember = writer.Position();
            const int numMembersWrittenBefore = numMembersWritten;

            if (member.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("#if {}\n", member.condition));
            }

            HYP_DEFER({
                if (!member.condition.Any())
                {
                    return;
                }

                if (numMembersWritten == numMembersWrittenBefore)
                {
                    writer.Seek(positionBeforeMember, /* truncate */ true);
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("#endif // {}\n", member.condition));
                }
            });

            String managedName = member.friendlyName;

            if (const ClassAttributeValue& attr = member.GetAttribute(Attributes::g_attrManagedName); attr.IsValid() && attr.IsString())
            {
                managedName = attr.GetString();
            }

            if (member.type == MemberType::Method)
            {
                if (member.name.StartsWith("operator"))
                {
                    continue;
                }

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
                Array<CSharpTypeMapping> paramTypeMappings;

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

                    const String parameterName = MapParameterName(parameter->name);

                    methodArgDecls.PushBack(HYP_FORMAT("{} {}", AppendNullable(parameterTypeMapping.typeName, parameterTypeMapping.isNullable), parameterName));
                    methodArgNames.PushBack(parameterName);
                    paramTypeMappings.PushBack(parameterTypeMapping);
                }

                const BindingsShared::MethodThunk* thunk = nullptr;

                for (const BindingsShared::MethodThunk& candidate : thunks)
                {
                    if (candidate.member == &member)
                    {
                        thunk = &candidate;
                        break;
                    }
                }

                if (thunk)
                {
                    String directBinding;

                    if (TryEmitDirectBinding(analyzer, cls, *thunk, managedName, libraryName, returnTypeMapping, paramTypeMappings, methodArgDecls, methodArgNames, directBinding))
                    {
                        writer.WriteString(directBinding);

                        ++numMembersWritten;

                        continue;
                    }
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

                if (member.cxxType->isStatic)
                {
                    writer.WriteString(HYP_FORMAT("            StaticField field = (StaticField)obj.Class.GetStaticField(new Name({}));\n", uint64(CreateStringHashFromDynamicString(member.friendlyName.Data()))));
                    writer.WriteString("            IntPtr fieldAddress = field.DataPointer;\n\n");
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("            Field field = (Field)obj.Class.GetField(new Name({}));\n", uint64(CreateStringHashFromDynamicString(member.friendlyName.Data()))));
                    writer.WriteString("            IntPtr fieldAddress = obj.NativeAddress + ((IntPtr)((Field)field).Offset);\n\n");
                }

                writer.WriteString("            return new ScriptableDelegate(obj, fieldAddress);\n");

                writer.WriteString("        }\n");

                ++numMembersWritten;

                continue;
            }
        }

        writer.WriteString("    }\n");

        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#endif // {}\n", cls.condition));
        }
    }

    writer.WriteString("}\n");

    return {};
}

} // namespace CodeGen
} // namespace Hyperion
