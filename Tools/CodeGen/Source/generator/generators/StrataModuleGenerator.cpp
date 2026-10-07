/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/StrataModuleGenerator.hpp>
#include <generator/generators/BindingsShared.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <Util/Util.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Utilities/StringUtil.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

namespace Hyperion {
namespace CodeGen {

HYP_DECLARE_LOG_CHANNEL(Tool);

using namespace BindingsShared;

FilePath StrataModuleGenerator::GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetStrataOutputDirectory() / relativePath.BasePath() / String(StringUtil::StripExtension(relativePath.Basename())) + ".strata";
}

Set<String> StrataModuleGenerator::CollectHandleNames(const Analyzer& analyzer) const
{
    Set<String> handleNames;

    // The base for all class types in the engine.
    handleNames.Insert("ObjectBase");

    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, ClassDefinition>& pair : mod->GetClasses())
        {
            const ClassDefinition& cls = pair.second;

            // Reflected classes (HYP_CLASS) are handles
            if (cls.type == ClassDefinitionType::Class && IsStrataScriptable(analyzer, cls))
            {
                handleNames.Insert(cls.name);
            }
        }
    }

    return handleNames;
}

String StrataModuleGenerator::ResolveHandleBase(const Analyzer& analyzer, const ClassDefinition& cls, const Set<String>& allHandleNames) const
{
    // ObjectBase is the root handle; it has no base.
    if (cls.name == "ObjectBase")
    {
        return String::empty;
    }

    // Nearest declared base handle (e.g. `handle Camera : Entity`).
    for (const String& baseName : cls.baseClassNames)
    {
        if (allHandleNames.Contains(baseName))
        {
            return baseName;
        }
    }

    // A direct ObjectBase base.
    for (const String& baseName : cls.baseClassNames)
    {
        if (baseName == "ObjectBase")
        {
            return "ObjectBase";
        }
    }

    // Fall back to ObjectBase when intermediate bases aren't exposed.
    if (analyzer.HasBaseClass(cls, "ObjectBase"))
    {
        return "ObjectBase";
    }

    return String::empty;
}

Set<String> StrataModuleGenerator::CollectEnumNames(const Analyzer& analyzer) const
{
    Set<String> enumNames;

    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, ClassDefinition>& pair : mod->GetClasses())
        {
            const ClassDefinition& cls = pair.second;

            if (cls.type == ClassDefinitionType::Enum && IsStrataScriptable(analyzer, cls))
            {
                enumNames.Insert(cls.name);
            }
        }
    }

    return enumNames;
}

Result StrataModuleGenerator::EmitEnums(const Analyzer& analyzer, ByteWriter& writer) const
{
    Array<const ClassDefinition*> enums;

    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, ClassDefinition>& pair : mod->GetClasses())
        {
            const ClassDefinition& cls = pair.second;

            if (cls.type == ClassDefinitionType::Enum && IsStrataScriptable(analyzer, cls))
            {
                enums.PushBack(&cls);
            }
        }
    }

    if (enums.Empty())
    {
        return {};
    }

    // Deterministic ordering for stable output (mirrors handle emission).
    std::sort(enums.Begin(), enums.End(), [](const ClassDefinition* a, const ClassDefinition* b)
        {
            if (a->staticIndex != b->staticIndex)
            {
                return a->staticIndex < b->staticIndex;
            }

            return a->name < b->name;
        });

    writer.WriteString("\n");

    for (const ClassDefinition* cls : enums)
    {
        if (cls->baseClassNames.Size() > 1)
        {
            return HYP_MAKE_ERROR(Error, "Enum '{}' may only have one underlying type", cls->name);
        }

        // C++ enums without an explicit underlying use `int`.
        const String underlying = cls->baseClassNames.Any()
            ? StrataEnumUnderlyingType(cls->baseClassNames.Front())
            : String("int");

        if (!underlying.Any())
        {
            return HYP_MAKE_ERROR(Error, "Enum '{}' has underlying type '{}' with no Strata equivalent (only the integral scalars are legal)",
                cls->name, cls->baseClassNames.Any() ? cls->baseClassNames.Front() : "<none>");
        }

        Array<String> memberDecls;

        for (const MemberDef& member : cls->members)
        {
            if (!MemberIsStrataScriptable(member) || member.type != MemberType::StaticField)
            {
                continue;
            }

            String memberDecl = ResolveManagedName(member);

            // Emit explicit values so bit-flag enums keep their C++ values.
            if (member.cxxDecl != nullptr && member.cxxDecl->value != nullptr)
            {
                memberDecl += HYP_FORMAT(" = {}", TranslateLimitMacros(member.cxxDecl->value->ToString()));
            }

            memberDecls.PushBack(memberDecl);
        }

        if (memberDecls.Empty())
        {
            continue;
        }

        writer.WriteString(HYP_FORMAT("enum {} : {}\n", cls->name, underlying) + "{\n");

        for (size_t i = 0; i < memberDecls.Size(); ++i)
        {
            writer.WriteString("    " + memberDecls[i] + (i + 1 < memberDecls.Size() ? ",\n" : "\n"));
        }

        writer.WriteString("}\n");
    }

    return {};
}

Set<String> StrataModuleGenerator::CollectForwardStructNames(const Analyzer& analyzer) const
{
    Set<String> structNames;

    // Reflected structs (HYP_STRUCT) are forward-declared as structs
    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, ClassDefinition>& pair : mod->GetClasses())
        {
            const ClassDefinition& cls = pair.second;

            if (cls.type == ClassDefinitionType::Struct && IsStrataScriptable(analyzer, cls))
            {
                structNames.Insert(cls.name);
            }
        }
    }

    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, ClassDefinition>& pair : mod->GetClasses())
        {
            const ClassDefinition& cls = pair.second;

            if (!IsStrataScriptable(analyzer, cls))
            {
                continue;
            }

            for (const MemberDef& member : cls.members)
            {
                if (!MemberIsStrataScriptable(member) || member.type != MemberType::Method)
                {
                    continue;
                }

                if (!member.cxxType || !member.cxxType->isFunction)
                {
                    continue;
                }

                const ASTFunctionType* functionType = dynamic_cast<const ASTFunctionType*>(member.cxxType.Get());

                if (!functionType)
                {
                    continue;
                }

                TResult<StrataTypeMapping> retRes = MapToStrataType(analyzer, functionType->returnType);

                if (!retRes.HasError() && retRes.GetValue().isStructValue)
                {
                    structNames.Insert(retRes.GetValue().typeName);
                }

                for (size_t j = 0; j < functionType->parameters.Size(); ++j)
                {
                    const ASTMemberDecl* param = functionType->parameters[j];

                    TResult<StrataTypeMapping> parRes = MapToStrataType(analyzer, param->type.Get());

                    if (!parRes.HasError() && parRes.GetValue().isStructValue)
                    {
                        structNames.Insert(parRes.GetValue().typeName);
                    }
                }
            }
        }
    }

    return structNames;
}

Result StrataModuleGenerator::EmitForwardStructDeclarations(const Analyzer& analyzer, const Set<String>& allStructNames, ByteWriter& writer) const
{
    if (allStructNames.Empty())
    {
        return {};
    }

    Array<String> sorted;
    sorted.Reserve(allStructNames.Size());

    for (const String& name : allStructNames)
    {
        sorted.PushBack(name);
    }

    std::sort(sorted.Begin(), sorted.End());

    for (const String& name : sorted)
    {
        writer.WriteString(HYP_FORMAT("struct {};\n", name));
    }

    return {};
}

Result StrataModuleGenerator::EmitHandles(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    const Set<String> allHandleNames = CollectHandleNames(analyzer);

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        if (cls.type != ClassDefinitionType::Class)
        {
            continue;
        }

        if (!IsStrataScriptable(analyzer, cls))
        {
            continue;
        }

        const String baseHandle = ResolveHandleBase(analyzer, cls, allHandleNames);

        if (baseHandle.Any())
        {
            writer.WriteString(HYP_FORMAT("handle {} : {};\n", cls.name, baseHandle));
        }
        else
        {
            writer.WriteString(HYP_FORMAT("handle {};\n", cls.name));
        }
    }

    return {};
}

Result StrataModuleGenerator::EmitMethods(const Analyzer& analyzer, const Module& mod, const Set<String>& allHandleNames, ByteWriter& writer) const
{
    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        if (cls.type != ClassDefinitionType::Class && cls.type != ClassDefinitionType::Struct
            && cls.type != ClassDefinitionType::Enum)
        {
            // Unknown types: nothing to emit here.
            continue;
        }

        if (!IsStrataScriptable(analyzer, cls))
        {
            continue;
        }

        // Handles and structs both take impl blocks; the struct receiver crosses
        // the ABI as a pointer.
        String classOutput;
        bool emittedAnyForClass = false;

        // Dedup extern symbols: overloads and property accessors must not collide.
        Set<String> emittedExternNames;

        auto ensureClassHeader = [&]()
        {
            if (emittedAnyForClass)
            {
                return;
            }

            if (cls.condition.Any())
            {
                classOutput += HYP_FORMAT("\n// {} (requires {})\n", cls.name, cls.condition);
            }
            else
            {
                classOutput += HYP_FORMAT("\n// {}\n", cls.name);
            }

            classOutput += HYP_FORMAT("impl {}", cls.name) + " {\n";

            emittedAnyForClass = true;
        };

        for (const MemberDef& member : cls.members)
        {
            if (!MemberIsStrataScriptable(member))
            {
                continue;
            }

            if (member.type != MemberType::Method)
            {
                continue;
            }

            if (!member.cxxType || !member.cxxType->isFunction)
            {
                continue;
            }

            const ASTFunctionType* functionType = dynamic_cast<const ASTFunctionType*>(member.cxxType.Get());

            if (!functionType)
            {
                continue;
            }

            const bool isStatic = member.cxxType->isStatic;

            // C++ enums cannot declare instance members
            if (cls.type == ClassDefinitionType::Enum && !isStatic)
            {
                continue;
            }

            // The C generator binds more than Strata can declare (owned handle returns, more than two parameters).
            // Reserve the symbol of a skipped method so a later overload can't be declared against its thunk.
            bool strataBindable = false;

            if (!ResolveMethodBindability(analyzer, cls, member, allHandleNames, strataBindable))
            {
                continue;
            }

            if (!strataBindable)
            {
                emittedExternNames.Insert(HYP_FORMAT("{}_{}", cls.name, ResolveManagedName(member)));

                continue;
            }

            // Unsupported return types skip the method.
            StrataTypeMapping returnTypeMapping;

            if (TResult<StrataTypeMapping> res = MapToStrataType(analyzer, functionType->returnType); res.HasError())
            {
                continue;
            }
            else
            {
                returnTypeMapping = res.GetValue();
            }

            // A handle/enum return must reference a declared type.
            if ((returnTypeMapping.isHandle || returnTypeMapping.isEnum)
                && !allHandleNames.Contains(returnTypeMapping.typeName))
            {
                continue;
            }

            // Map each parameter; any unsupported type skips the method.
            Array<String> paramDecls;

            if (!isStatic)
            {
                paramDecls.PushBack(SelfParamDecl(cls, functionType->isConstMethod));
            }

            bool paramsOk = true;

            for (size_t j = 0; j < functionType->parameters.Size(); ++j)
            {
                const ASTMemberDecl* parameter = functionType->parameters[j];

                TResult<StrataTypeMapping> paramRes = MapToStrataType(analyzer, parameter->type.Get());

                if (paramRes.HasError())
                {
                    paramsOk = false;
                    break;
                }

                StrataTypeMapping paramTypeMapping = paramRes.GetValue();

                if ((paramTypeMapping.isHandle || paramTypeMapping.isEnum)
                    && !allHandleNames.Contains(paramTypeMapping.typeName))
                {
                    paramsOk = false;
                    break;
                }

                if (j >= 2)
                {
                    paramsOk = false;
                    break;
                }

                String paramName = parameter->name.Any() ? parameter->name : HYP_FORMAT("arg{}", j);

                if (paramTypeMapping.isString)
                {
                    // Read-only; the caller's string isn't consumed.
                    paramDecls.PushBack(HYP_FORMAT("const {} {}", paramTypeMapping.typeName, paramName));
                }
                else if (paramTypeMapping.isArray)
                {
                    // Read-only; the caller's array isn't consumed.
                    paramDecls.PushBack(HYP_FORMAT("const ref {} {}", paramTypeMapping.typeName, paramName));
                }
                else if (paramTypeMapping.isStructValue)
                {
                    paramDecls.PushBack(HYP_FORMAT("{}{} {}", StructParamModifier(parameter->type.Get()), paramTypeMapping.typeName, paramName));
                }
                else if (paramTypeMapping.isVector)
                {
                    // float2/float3/float4 cross by pointer, so the same thunk serves plain C callers.
                    paramDecls.PushBack(HYP_FORMAT("const ref {} {}", paramTypeMapping.typeName, paramName));
                }
                else
                {
                    // Scalars pass by value.
                    paramDecls.PushBack(HYP_FORMAT("{} {}", paramTypeMapping.typeName, paramName));
                }
            }

            if (!paramsOk)
            {
                continue;
            }

            // Structs, arrays and strings can't be extern returns.
            // Need to go through return-params
            const bool propertySetterIdiom = IsPropertySetterIdiom(cls, member);
            const bool returnsViaOutParam = !propertySetterIdiom
                && (returnTypeMapping.isStructValue || returnTypeMapping.isArray || returnTypeMapping.isString || returnTypeMapping.isVector);
            String strataReturnType = returnTypeMapping.typeName;

            if (propertySetterIdiom)
            {
                strataReturnType = "void";
            }
            else if (returnsViaOutParam)
            {
                strataReturnType = "void";
                paramDecls.PushBack(HYP_FORMAT("return {} outReturn", returnTypeMapping.typeName));
            }

            const String managedName = ResolveManagedName(member);
            // The Strata parser renames impl methods to Type_Method.
            const String externName = HYP_FORMAT("{}_{}", cls.name, managedName);

            // Skip overloads that would collide with an already-emitted symbol.
            if (emittedExternNames.Contains(externName))
            {
                continue;
            }

            ensureClassHeader();

            emittedExternNames.Insert(externName);

            const String paramsString = paramDecls.Any() ? String::Join(paramDecls, ", ") : String("");

            // Strata has no preprocessor; record the condition as a comment.
            if (member.condition.Any())
            {
                classOutput += HYP_FORMAT("    // requires: {}\n", member.condition);
            }

            classOutput += HYP_FORMAT("    extern {} {}({});\n", strataReturnType, managedName, paramsString);
        }

        // Properties on handles and structs (enums have no instance members).
        if (cls.type != ClassDefinitionType::Enum)
        {
            for (const ResolvedStrataProperty& prop : CollectImplProperties(analyzer, cls, allHandleNames))
            {
                PropertyAccessorBinding binding;

                if (!ResolvePropertyAccessorBinding(analyzer, cls, prop, allHandleNames, emittedExternNames, binding))
                {
                    continue;
                }

                ensureClassHeader();

                // Strata has no preprocessor; record the condition as a comment.
                if (prop.condition.Any())
                {
                    classOutput += HYP_FORMAT("    // requires: {}\n", prop.condition);
                }

                String accessors;

                if (binding.getterSymbol.Any())
                {
                    // Synthesized string and vector getters need an explicit extern decl
                    // (they return through an out-param).
                    if (binding.getterSynthesized && (prop.mapping.isString || prop.mapping.isVector))
                    {
                        classOutput += HYP_FORMAT("    extern void {}_Get_{}({}, return {} outReturn);\n",
                            cls.name, prop.name, SelfParamDecl(cls, true), prop.mapping.typeName);
                    }

                    accessors += HYP_FORMAT("get = {};", binding.getterSymbol);

                    if (binding.getterSynthesized)
                    {
                        emittedExternNames.Insert(binding.getterSymbol);
                    }
                }

                if (binding.setterSymbol.Any())
                {
                    // A synthesized vector setter takes its value by pointer.
                    if (binding.setterSynthesized && prop.mapping.isVector)
                    {
                        classOutput += HYP_FORMAT("    extern void {}_Set_{}({}, const ref {} value);\n",
                            cls.name, prop.name, SelfParamDecl(cls, false), prop.mapping.typeName);
                    }

                    accessors += HYP_FORMAT("{}set = {};", accessors.Any() ? " " : "", binding.setterSymbol);

                    if (binding.setterSynthesized)
                    {
                        emittedExternNames.Insert(binding.setterSymbol);
                    }
                }

                classOutput += HYP_FORMAT("    property {} {} ", prop.mapping.typeName, prop.name) + "{ " + accessors + " }\n";
            }
        }

        if (emittedAnyForClass)
        {
            classOutput += "}\n";

            writer.WriteString(classOutput);
        }
    }

    return {};
}

Result StrataModuleGenerator::Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    // Emit this module's handles, enums, struct forward declarations, then methods.
    Set<String> moduleHandles;

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        if ((cls.type == ClassDefinitionType::Class || cls.type == ClassDefinitionType::Struct)
            && IsStrataScriptable(analyzer, cls))
        {
            moduleHandles.Insert(cls.name);
        }
    }

    // Enums must be declared types before the impl blocks.
    for (const String& enumName : CollectEnumNames(analyzer))
    {
        moduleHandles.Insert(enumName);
    }

    if (Result res = EmitHandles(analyzer, mod, writer); res.HasError())
    {
        return res;
    }

    if (Result res = EmitEnums(analyzer, writer); res.HasError())
    {
        return res;
    }

    // Forward-declare struct types so impl blocks can reference them.
    if (Result res = EmitForwardStructDeclarations(analyzer, CollectForwardStructNames(analyzer), writer); res.HasError())
    {
        return res;
    }

    return EmitMethods(analyzer, mod, moduleHandles, writer);
}

} // namespace CodeGen
} // namespace Hyperion
