/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/StrataModuleGenerator.hpp>

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

namespace Hyperion {
namespace CodeGen {

HYP_DECLARE_LOG_CHANNEL(Tool);

namespace {

bool IsStrataScriptable(const Analyzer& analyzer, const ClassDefinition& cls)
{
    if (analyzer.HasAttrInHierarchy(cls, Attributes::g_attrNoScriptBindings))
    {
        return false;
    }

    if (const ClassAttributeValue& attr = cls.GetAttribute(Attributes::g_attrOnlyLanguages); attr.IsValid() && attr.IsString())
    {
        if (!CheckAttrCSV(attr, "strata"))
        {
            return false;
        }
    }

    return true;
}

bool MemberIsStrataScriptable(const MemberDef& member)
{
    if (const ClassAttributeValue& attr = member.GetAttribute(Attributes::g_attrNoScriptBindings); attr.GetBool())
    {
        return false;
    }

    // "Scriptable" methods don't pertain to strata.
    if (member.name.EndsWith("_Impl"))
    {
        return false;
    }

    // Operator overloads (operator==, operator!=, ...) have no valid Strata
    // identifier spelling - the extern name would embed the operator symbol
    // itself (e.g. "Transform_operator=="), which the Strata parser rejects.
    // The C# generator skips these too for the same reason.
    if (member.name.StartsWith("operator"))
    {
        return false;
    }

    if (const ClassAttributeValue& attr = member.GetAttribute(Attributes::g_attrOnlyLanguages); attr.IsValid() && attr.IsString())
    {
        if (!CheckAttrCSV(attr, "strata"))
        {
            return false;
        }
    }

    return true;
}

String ResolveManagedName(const MemberDef& member)
{
    if (const ClassAttributeValue& attr = member.GetAttribute(Attributes::g_attrManagedName); attr.IsValid() && attr.IsString())
    {
        return attr.GetString();
    }

    return member.friendlyName;
}

// Strata struct parameters are references; write `const` for a read-only view.
String StructParamModifier(const ASTType* paramType)
{
    if (paramType == nullptr)
    {
        return "const ";
    }

    bool isConst = true;

    if (paramType->isPointer)
    {
        isConst = paramType->ptrTo && paramType->ptrTo->isConst;
    }
    else if (paramType->isLvalueReference)
    {
        isConst = paramType->refTo && paramType->refTo->isConst;
    }

    return isConst ? "const " : String::empty;
}

} // anonymous namespace

FilePath StrataModuleGenerator::GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    FilePath relativePath = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetStrataOutputDirectory() / relativePath.BasePath() / StringUtil::StripExtension(relativePath.Basename()) + ".strata";
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

    // Prefer the nearest declared (scriptable) base handle, so derived handles
    // extend their actual engine base (e.g. Camera extends Entity).
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

    // If the type is ultimately derived from ObjectBase through intermediate
    // bases that aren't exposed as Strata handles, still declare it as extending
    // ObjectBase so the is-a relationship (and up/down casts) are preserved.
    if (analyzer.HasBaseClass(cls, "ObjectBase"))
    {
        return "ObjectBase";
    }

    return String::empty;
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

                // Check return type.
                TResult<StrataTypeMapping> retRes = MapToStrataType(analyzer, functionType->returnType);

                if (!retRes.HasError() && retRes.GetValue().isStructValue)
                {
                    structNames.Insert(retRes.GetValue().typeName);
                }

                // Check each parameter type.
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

        const String extendsBase = ResolveHandleBase(analyzer, cls, allHandleNames);

        if (extendsBase.Any())
        {
            writer.WriteString(HYP_FORMAT("handle {} extends {};\n", cls.name, extendsBase));
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

        if (cls.type != ClassDefinitionType::Class && cls.type != ClassDefinitionType::Struct)
        {
            // Enums / unknown types: Strata has no enum or aggregate constant
            // form (YET), so nothing to emit here.
            continue;
        }

        if (!IsStrataScriptable(analyzer, cls))
        {
            continue;
        }

        bool emittedAnyForClass = false;

        // Track emitted extern names so C++ overloads (same method name, different
        // params) don't produce duplicate Strata symbols, which would fail to compile.
        Set<String> emittedExternNames;

        for (size_t i = 0; i < cls.members.Size(); ++i)
        {
            const MemberDef& member = cls.members[i];

            if (!MemberIsStrataScriptable(member))
            {
                continue;
            }

            // v1 binds methods only. Strata cannot represent object state (fields)
            // without get/set free functions, so fields and properties are skipped.
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

            // Map the return type. Unsupported return types skip the method.
            StrataTypeMapping returnTypeMapping;

            if (TResult<StrataTypeMapping> res = MapToStrataType(analyzer, functionType->returnType); res.HasError())
            {
                continue;
            }
            else
            {
                returnTypeMapping = res.GetValue();
            }

            // A handle return must reference a declared handle.
            if (returnTypeMapping.isHandle && !allHandleNames.Contains(returnTypeMapping.typeName))
            {
                continue;
            }

            // Map each parameter. Any unsupported type, or handle referencing an
            // undeclared type, skips the method.
            Array<String> paramDecls;

            if (!isStatic)
            {
                // Instance methods receive their object as the first handle param.
                paramDecls.PushBack(HYP_FORMAT("{} self", cls.name));
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

                if (paramTypeMapping.isHandle && !allHandleNames.Contains(paramTypeMapping.typeName))
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
                    // Read-only: avoids the caller's string being moved/consumed by the call.
                    paramDecls.PushBack(HYP_FORMAT("const {} {}", paramTypeMapping.typeName, paramName));
                }
                else if (paramTypeMapping.isArray)
                {
                    // Read-only reference: avoids the caller's array being moved/consumed.
                    paramDecls.PushBack(HYP_FORMAT("const ref {} {}", paramTypeMapping.typeName, paramName));
                }
                else if (paramTypeMapping.isStructValue)
                {
                    paramDecls.PushBack(HYP_FORMAT("{}{} {}", StructParamModifier(parameter->type.Get()), paramTypeMapping.typeName, paramName));
                }
                else
                {
                    // Scalars and vector types (float3/float4) pass by value.
                    paramDecls.PushBack(HYP_FORMAT("{} {}", paramTypeMapping.typeName, paramName));
                }
            }

            if (!paramsOk)
            {
                continue;
            }

            // Strata cannot return aggregates ABI boundary, so we use a ref out param.
            const bool returnsViaOutParam = returnTypeMapping.isStructValue || returnTypeMapping.isArray || returnTypeMapping.isVector;
            String strataReturnType = returnTypeMapping.typeName;

            if (returnsViaOutParam)
            {
                strataReturnType = "void";
                paramDecls.PushBack(HYP_FORMAT("ref {} outReturn", returnTypeMapping.typeName));
            }

            const String managedName = ResolveManagedName(member);
            const String externName = HYP_FORMAT("{}_{}", cls.name, managedName);

            // Skip C++ overloads that would collide with an already-emitted symbol.
            if (emittedExternNames.Contains(externName))
            {
                continue;
            }

            if (!emittedAnyForClass)
            {
                if (cls.condition.Any())
                {
                    writer.WriteString(HYP_FORMAT("\n// {} (requires {})\n", cls.name, cls.condition));
                }
                else
                {
                    writer.WriteString(HYP_FORMAT("\n// {}\n", cls.name));
                }

                emittedAnyForClass = true;
            }

            emittedExternNames.Insert(externName);

            const String paramsString = paramDecls.Any() ? String::Join(paramDecls, ", ") : String("");

            // Strata has no preprocessor; record a method-level condition (e.g.
            // EditorOnly) as a comment. The matching C++ thunk is #if-guarded, so
            // where the condition is false the extern simply won't be registered.
            if (member.condition.Any())
            {
                writer.WriteString(HYP_FORMAT("// requires: {}\n", member.condition));
            }

            writer.WriteString(HYP_FORMAT("extern {} {}({});\n", strataReturnType, externName, paramsString));
        }
    }

    return {};
}

Result StrataModuleGenerator::EmitThunks(const Analyzer& analyzer, const Module& mod, const Set<String>& allHandleNames, ByteWriter& writer) const
{
    bool openedBlock = false;

    HYP_DEFER({
        if (openedBlock)
        {
            writer.WriteString("#endif // HYP_STRATA\n");
        }
    });

    auto ensureStrataHeader = [&]()
    {
        if (!openedBlock)
        {
            writer.WriteString("\n#ifdef HYP_STRATA\n\n");
            writer.WriteString("#include <Core/Scripting/Strata/ThunkDrawer.hpp>\n");
            writer.WriteString("#include <Core/Scripting/Strata/StrataMarshal.hpp>\n\n");

            openedBlock = true;
        }
    };

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        if ((cls.type != ClassDefinitionType::Class && cls.type != ClassDefinitionType::Struct)
            || !IsStrataScriptable(analyzer, cls))
        {
            continue;
        }

        Set<String> emittedExternNames;
        String classThunks;

        for (size_t i = 0; i < cls.members.Size(); ++i)
        {
            const MemberDef& member = cls.members[i];

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

            const bool isStatic = member.cxxType->isStatic;

            // Predicate must match EmitMethods exactly. Unsupported types skip.
            StrataTypeMapping returnTypeMapping;

            if (TResult<StrataTypeMapping> res = MapToStrataType(analyzer, functionType->returnType); res.HasError())
            {
                continue;
            }
            else
            {
                returnTypeMapping = res.GetValue();
            }

            if (returnTypeMapping.isHandle && !allHandleNames.Contains(returnTypeMapping.typeName))
            {
                continue;
            }

            if (functionType->parameters.Size() > 2)
            {
                continue;
            }

            Array<String> sigParams; // "Type name" for the thunk signature
            Array<String> callArgs;  // "name" for the forwarded call
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

                const StrataTypeMapping paramTypeMapping = paramRes.GetValue();

                if (paramTypeMapping.isHandle && !allHandleNames.Contains(paramTypeMapping.typeName))
                {
                    paramsOk = false;

                    break;
                }

                String paramName = parameter->name.Any() ? parameter->name : HYP_FORMAT("arg{}", j);

                // The parameter type with any top-level reference stripped: Strata's
                // string/array/vector values don't cross the boundary as C++
                // references, even though the C++ signature conventionally takes
                // them by const&.
                const ASTType* unwrappedParamType = (parameter->type->isLvalueReference || parameter->type->isRvalueReference)
                    ? parameter->type->refTo.Get()
                    : parameter->type.Get();

                if (paramTypeMapping.isString)
                {
                    // Strata's `string` has no attached length; extern params receive
                    // the raw char* directly (see the host-boundary note in strings.strata).
                    const String stringCxxTypeName = unwrappedParamType->typeName->ToString(/* includeNamespace */ false);

                    sigParams.PushBack(HYP_FORMAT("const char* {}", paramName));
                    callArgs.PushBack(HYP_FORMAT("{}({})", stringCxxTypeName, paramName));
                }
                else if (paramTypeMapping.isArray)
                {
                    // Strata hands us a pointer to its fat {ptr, u64} array
                    // representation; copy it into an engine array to forward.
                    const String elementCxxType = unwrappedParamType->templateArguments[0]->type->Format();
                    const String arrayCxxTypeName = unwrappedParamType->typeName->ToString(/* includeNamespace */ false);

                    sigParams.PushBack(HYP_FORMAT("::Hyperion::Strata::ArrayView<{}>* {}", elementCxxType, paramName));
                    callArgs.PushBack(HYP_FORMAT("{}<{}>({}->data, size_t({}->length))", arrayCxxTypeName, elementCxxType, paramName, paramName));
                }
                else if (paramTypeMapping.isVector)
                {
                    // float3/float4 cross the extern "C" boundary as a raw SIMD
                    // register (stratac lowers them to __m128/float32x4_t), not
                    // a Vec3f/Vec4f struct by value - convert explicitly.
                    const String vectorCxxTypeName = unwrappedParamType->typeName->ToString(/* includeNamespace */ false);
                    const String convertFnName = vectorCxxTypeName == "Vec4f" ? "SimdVectorToVec4f" : "SimdVectorToVec3f";

                    sigParams.PushBack(HYP_FORMAT("::Hyperion::Strata::SimdVector {}", paramName));
                    callArgs.PushBack(HYP_FORMAT("::Hyperion::Strata::{}({})", convertFnName, paramName));
                }
                else if (paramTypeMapping.isStructValue)
                {
                    // Strata hands us a pointer; forward/deref based on the C++ signature.
                    sigParams.PushBack(HYP_FORMAT("{}* {}", paramTypeMapping.typeName, paramName));
                    callArgs.PushBack(parameter->type->isPointer ? paramName : HYP_FORMAT("*{}", paramName));
                }
                else
                {
                    sigParams.PushBack(parameter->type->FormatDecl(paramName));
                    callArgs.PushBack(paramName);
                }
            }

            if (!paramsOk)
            {
                continue;
            }

            const String managedName = ResolveManagedName(member);
            const String externName = HYP_FORMAT("{}_{}", cls.name, managedName);

            if (emittedExternNames.Contains(externName))
            {
                continue;
            }

            emittedExternNames.Insert(externName);

            const bool returnsViaOutParam = returnTypeMapping.isStructValue
                || returnTypeMapping.isArray
                || returnTypeMapping.isVector;

            // Build the signature param list: [<Class>* self, ]<params...>[, <Ret>(*) outReturn]
            Array<String> allSigParams;

            if (!isStatic)
            {
                allSigParams.PushBack(HYP_FORMAT("{}* self", cls.name));
            }

            for (const String& sigParam : sigParams)
            {
                allSigParams.PushBack(sigParam);
            }

            if (returnTypeMapping.isArray)
            {
                // Same reference-stripping as params: a getter conventionally
                // returns `const Array<T>&`, whose template argument lives on
                // the referenced type, not the reference wrapper itself.
                const ASTType* unwrappedReturnType = (functionType->returnType->isLvalueReference || functionType->returnType->isRvalueReference)
                    ? functionType->returnType->refTo.Get()
                    : functionType->returnType.Get();

                const String returnElementCxxType = unwrappedReturnType->templateArguments[0]->type->Format();

                allSigParams.PushBack(HYP_FORMAT("::Hyperion::Strata::ArrayView<{}>* outReturn", returnElementCxxType));
            }
            else if (returnTypeMapping.isVector)
            {
                // returnTypeMapping.typeName is the Strata name (float3/float4),
                // not a C++ type - the out param is the raw SIMD register type.
                allSigParams.PushBack("::Hyperion::Strata::SimdVector* outReturn");
            }
            else if (returnsViaOutParam)
            {
                allSigParams.PushBack(HYP_FORMAT("{}* outReturn", returnTypeMapping.typeName));
            }

            const String sigParamsString = allSigParams.Any() ? String::Join(allSigParams, ", ") : String("");
            const String callArgsString = callArgs.Any() ? String::Join(callArgs, ", ") : String("");

            const String callExpr = isStatic
                ? HYP_FORMAT("{}::{}({})", cls.name, member.name, callArgsString)
                : HYP_FORMAT("self->{}({})", member.name, callArgsString);

            // Build this method's thunk + registrar first, then wrap the pair in
            // the method's own preprocessor condition (EditorOnly / Condition
            // attribute, or a path-derived define). This mirrors how the CXX
            // generator guards member reflection, so a thunk only compiles when
            // the method it forwards to actually exists.
            String methodOutput;

            if (returnTypeMapping.isArray)
            {
                methodOutput += HYP_FORMAT("extern \"C\" void {}({})", externName, sigParamsString);
                methodOutput += " { const auto& hypReturnValue = ";
                methodOutput += callExpr;
                methodOutput += "; ::Hyperion::Strata::SetReturnArray(outReturn, hypReturnValue.Data(), hypReturnValue.Size()); }\n";
            }
            else if (returnTypeMapping.isStructValue)
            {
                methodOutput += HYP_FORMAT("extern \"C\" void {}({})", externName, sigParamsString);
                methodOutput += " { *outReturn = ";
                methodOutput += callExpr;
                methodOutput += "; }\n";
            }
            else if (returnTypeMapping.isString)
            {
                methodOutput += HYP_FORMAT("extern \"C\" char* {}({})", externName, sigParamsString);
                methodOutput += " { return ::Hyperion::Strata::AllocReturnString(";
                methodOutput += callExpr;
                methodOutput += "); }\n";
            }
            else if (functionType->returnType->IsVoid())
            {
                methodOutput += HYP_FORMAT("extern \"C\" void {}({})", externName, sigParamsString);
                methodOutput += " { ";
                methodOutput += callExpr;
                methodOutput += "; }\n";
            }
            else if (returnTypeMapping.isVector)
            {
                methodOutput += HYP_FORMAT("extern \"C\" void {}({})", externName, sigParamsString);
                methodOutput += " { *outReturn = ::Hyperion::Strata::ToSimdVector(";
                methodOutput += callExpr;
                methodOutput += "); }\n";
            }
            else
            {
                const String returnTypeString = functionType->returnType->Format();

                methodOutput += HYP_FORMAT("extern \"C\" {} {}({})", returnTypeString, externName, sigParamsString);
                methodOutput += " { return ";
                methodOutput += callExpr;
                methodOutput += "; }\n";
            }

            // Register for static init.
            methodOutput += HYP_FORMAT("static const bool s_strataBinding_{}_{} = ::Hyperion::Strata::ThunkDrawer::Register(\"{}\"_sh, reinterpret_cast<void*>(&{}_{}));\n",
                cls.name, managedName, externName, cls.name, managedName);

            if (member.condition.Any())
            {
                classThunks += HYP_FORMAT("#if {}\n", member.condition);
            }

            classThunks += methodOutput;

            if (member.condition.Any())
            {
                classThunks += HYP_FORMAT("#endif // {}\n", member.condition);
            }

            classThunks += "\n";
        }

        if (!classThunks.Any())
        {
            continue;
        }

        ensureStrataHeader();

        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#if {}\n\n", cls.condition));
        }

        writer.WriteString(classThunks);

        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#endif // {}\n\n", cls.condition));
        }
    }

    return {};
}

Result StrataModuleGenerator::Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    // Standalone per-module file: emit this module's handles, then its methods.
    // allHandleNames is restricted to this module, so methods referencing a handle
    // declared in another module are skipped (they would be undeclared here). The
    // merged-file dispatch path builds the global handle set and is preferred.
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

    if (Result res = EmitHandles(analyzer, mod, writer); res.HasError())
    {
        return res;
    }

    return EmitMethods(analyzer, mod, moduleHandles, writer);
}

} // namespace CodeGen
} // namespace Hyperion
