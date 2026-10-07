/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/CBindingGenerator.hpp>
#include <generator/generators/BindingsShared.hpp>
#include <generator/generators/BindingLayouts.hpp>

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
#include <string>
#include <cctype>
#include <utility>

namespace Hyperion {
namespace CodeGen {

using namespace BindingsShared;

namespace {

// Maps an extern param to its thunk signature param and forwarded call argument.
TResult<StrataTypeMapping> BuildThunkInputParam(const Analyzer& analyzer, const Set<String>& allHandleNames,
    const ASTType* paramType, const String& paramName, Array<String>& sigParams, Array<String>& callArgs)
{
    TResult<StrataTypeMapping> paramRes = MapToStrataType(analyzer, paramType);

    if (paramRes.HasError())
    {
        return paramRes;
    }

    const StrataTypeMapping paramTypeMapping = paramRes.GetValue();

    if (paramTypeMapping.isResult)
    {
        return HYP_MAKE_ERROR(Error, "Result can only be bound as a return value");
    }

    if ((paramTypeMapping.isHandle || paramTypeMapping.isEnum)
        && !allHandleNames.Contains(paramTypeMapping.typeName))
    {
        return HYP_MAKE_ERROR(Error, "Type '{}' is not a declared Strata {}", paramTypeMapping.typeName,
            paramTypeMapping.isEnum ? "enum" : "handle");
    }

    // Strip top-level references: string/array/vector values cross as values,
    // not as the conventional C++ const&.
    const ASTType* unwrappedParamType = (paramType->isLvalueReference || paramType->isRvalueReference)
        ? paramType->refTo.Get()
        : paramType;

    if (paramTypeMapping.isHandleWrapper)
    {
        // Borrowed: the callee takes its own reference if it keeps the object.
        sigParams.PushBack(HYP_FORMAT("{}* {}", paramTypeMapping.CxxTypeName(), paramName));
        callArgs.PushBack(HYP_FORMAT("::Hyperion::MakeStrongRef({})", paramName));
    }
    else if (paramTypeMapping.isString)
    {
        // Strata strings have no length; pass the raw char*.
        const String stringCxxTypeName = unwrappedParamType->typeName->ToString(/* includeNamespace */ false);

        sigParams.PushBack(HYP_FORMAT("const char* {}", paramName));
        callArgs.PushBack(HYP_FORMAT("{}({})", stringCxxTypeName, paramName));
    }
    else if (paramTypeMapping.isArray)
    {
        // Copy the {ptr, u64} fat into an engine array.
        const String elementCxxType = unwrappedParamType->templateArguments[0]->type->Format();
        const String arrayCxxTypeName = unwrappedParamType->typeName->ToString(/* includeNamespace */ false);

        sigParams.PushBack(HYP_FORMAT("::Hyperion::Strata::SArray<{}>* {}", elementCxxType, paramName));
        callArgs.PushBack(HYP_FORMAT("{}<{}>({}->data, size_t({}->length))", arrayCxxTypeName, elementCxxType, paramName, paramName));
    }
    else if (paramTypeMapping.isVector)
    {
        // Vec2f/Vec3f/Vec4f cross by pointer like any other struct, so the thunk is plain C.
        sigParams.PushBack(HYP_FORMAT("const {}* {}", paramTypeMapping.CxxTypeName(), paramName));
        callArgs.PushBack(HYP_FORMAT("*{}", paramName));
    }
    else if (paramTypeMapping.isStructValue)
    {
        sigParams.PushBack(HYP_FORMAT("{}* {}", paramTypeMapping.CxxTypeName(), paramName));
        callArgs.PushBack(paramType->isPointer ? paramName : HYP_FORMAT("*{}", paramName));
    }
    else
    {
        sigParams.PushBack(paramType->FormatDecl(paramName));
        callArgs.PushBack(paramName);
    }

    return paramRes;
}

// Formats the `extern "C"` thunk forwarding callExpr to the host boundary.
String FormatThunkDefinition(const String& externName, const StrataTypeMapping& returnTypeMapping,
    const ASTType* cxxReturnType, const String& sigParamsString, const String& callExpr, bool retainedReturn = false)
{
    String methodOutput;

    if (returnTypeMapping.isResult && returnTypeMapping.hasResultValue)
    {
        // Returns whether it succeeded. The value goes to outValue on success, the message to outError on failure.
        methodOutput += HYP_FORMAT("extern \"C\" bool {}({})", externName, sigParamsString);
        methodOutput += " { auto hypResult = ";
        methodOutput += callExpr;
        methodOutput += "; if (hypResult.HasError()) { ::Hyperion::Strata::SetReturnError(outError, hypResult.GetError().GetMessage()); return false; } ";

        if (returnTypeMapping.isHandleWrapper)
        {
            // the caller takes over the reference
            methodOutput += "auto hypValue = std::move(hypResult.GetValue()); *outValue = hypValue.Get(); hypValue.ptr = nullptr; ";
        }
        else if (returnTypeMapping.isString)
        {
            methodOutput += "::Hyperion::Strata::SetReturnString(outValue, hypResult.GetValue()); ";
        }
        else
        {
            methodOutput += "*outValue = hypResult.GetValue(); ";
        }

        methodOutput += "return true; }\n";
    }
    else if (returnTypeMapping.isArray)
    {
        methodOutput += HYP_FORMAT("extern \"C\" void {}({})", externName, sigParamsString);
        methodOutput += " { const auto& hypReturnValue = ";
        methodOutput += callExpr;
        methodOutput += "; ::Hyperion::Strata::SetReturnArray(outReturn, hypReturnValue.Data(), hypReturnValue.Size()); }\n";
    }
    else if (returnTypeMapping.isStructValue || returnTypeMapping.isVector)
    {
        methodOutput += HYP_FORMAT("extern \"C\" void {}({})", externName, sigParamsString);
        methodOutput += " { *outReturn = ";
        methodOutput += callExpr;
        methodOutput += "; }\n";
    }
    else if (returnTypeMapping.isString)
    {
        // `string` returns come back through the out-param ({ptr, len} fat).
        methodOutput += HYP_FORMAT("extern \"C\" void {}({})", externName, sigParamsString);
        methodOutput += " { ::Hyperion::Strata::SetReturnString(outReturn, ";
        methodOutput += callExpr;
        methodOutput += "); }\n";
    }
    else if (returnTypeMapping.isResult)
    {
        // Returns whether it succeeded; on failure the message goes to outError (which may be null).
        methodOutput += HYP_FORMAT("extern \"C\" bool {}({})", externName, sigParamsString);
        methodOutput += " { const auto hypResult = ";
        methodOutput += callExpr;
        methodOutput += "; if (hypResult.HasError()) { ::Hyperion::Strata::SetReturnError(outError, hypResult.GetError().GetMessage()); return false; } return true; }\n";
    }
    else if (cxxReturnType == nullptr || cxxReturnType->IsVoid())
    {
        methodOutput += HYP_FORMAT("extern \"C\" void {}({})", externName, sigParamsString);
        methodOutput += " { ";
        methodOutput += callExpr;
        methodOutput += "; }\n";
    }
    else if (returnTypeMapping.isHandleWrapper)
    {
        // Handle<T> crosses as T*. A handle returned by value carries its reference out with it (the caller releases
        // it with Hyp_Release); one returned by reference stays owned by the object it came from.
        methodOutput += HYP_FORMAT("extern \"C\" {}* {}({})", returnTypeMapping.CxxTypeName(), externName, sigParamsString);

        if (retainedReturn)
        {
            methodOutput += " { auto hypReturnValue = ";
            methodOutput += callExpr;
            methodOutput += "; auto* hypReturnPointer = hypReturnValue.Get(); hypReturnValue.ptr = nullptr; return hypReturnPointer; }\n";
        }
        else
        {
            methodOutput += " { return (";
            methodOutput += callExpr;
            methodOutput += ").Get(); }\n";
        }
    }
    else
    {
        const String returnTypeString = cxxReturnType->Format();

        methodOutput += HYP_FORMAT("extern \"C\" {} {}({})", returnTypeString, externName, sigParamsString);
        methodOutput += " { return ";
        methodOutput += callExpr;
        methodOutput += "; }\n";
    }

    return methodOutput;
}

// The manifest names types as C++ does; float2/float3/float4 are Strata's names for the engine vectors.
String ThunkTypeName(const StrataTypeMapping& mapping)
{
    return mapping.isVector ? mapping.CxxTypeName() : mapping.typeName;
}

// How a value crosses the thunk boundary, as recorded in the binding manifest.
String ThunkInputConvention(const StrataTypeMapping& mapping)
{
    if (mapping.isString)
    {
        return "cstring";
    }

    if (mapping.isArray)
    {
        return "array_ptr";
    }

    if (mapping.isVector)
    {
        return "struct_ptr";
    }

    if (mapping.isStructValue)
    {
        return "struct_ptr";
    }

    return mapping.isHandle ? "handle" : "value";
}

String ThunkReturnConvention(const StrataTypeMapping& mapping)
{
    if (mapping.isResult)
    {
        return "result";
    }

    if (mapping.typeName == "void")
    {
        return "void";
    }

    if (mapping.isArray)
    {
        return "array_out";
    }

    if (mapping.isString)
    {
        return "string_out";
    }

    if (mapping.isStructValue)
    {
        return "struct_out";
    }

    if (mapping.isVector)
    {
        return "struct_out";
    }

    return mapping.isHandle ? "handle" : "value";
}

String FormatBindingManifestValue(const ThunkBindingValue& value, bool includeName)
{
    String result = "{ ";

    if (includeName)
    {
        result += "\"name\": \"" + value.name + "\", ";
    }

    result += "\"type\": \"" + value.type + "\", \"convention\": \"" + value.convention + "\" }";

    return result;
}

} // anonymous namespace

Result CBindingGenerator::EmitLayoutChecks(const Module& mod, const BindingLayoutSet& layouts, ByteWriter& writer) const
{
    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        if (cls.type != ClassDefinitionType::Struct || !layouts.IsGenerated(cls.name))
        {
            continue;
        }

        const BindingLayout* layout = const_cast<BindingLayoutSet&>(layouts).Resolve(cls.name);

        const String tag = "CBindingLayout_" + cls.name;
        const String message = "\"The C binding layout of " + cls.name + " no longer matches the struct. Every data member must be a HYP_FIELD of a plain type; "
            + "otherwise mark it HYP_STRUCT(NoBindingLayout) so it stays opaque to C and Rust.\"";

        String output = "\n#include <cstddef>\n\n";

        if (cls.condition.Any())
        {
            output += "#if " + cls.condition + "\n";
        }

        // a TClassStaticInit specialization is a friend of the struct, so offsetof can see non-public fields
        output += "namespace Hyperion {\nstruct " + tag + ";\ntemplate <>\nclass TClassStaticInit<" + tag + "> final\n{\n";
        output += "    static_assert(sizeof(" + cls.name + ") == " + String(std::to_string(layout->size).c_str())
            + " && alignof(" + cls.name + ") == " + String(std::to_string(layout->align).c_str()) + ", " + message + ");\n";

        for (const BindingLayoutField& field : layout->fields)
        {
            output += "    static_assert(offsetof(" + cls.name + ", " + field.cxxName + ") == " + String(std::to_string(field.offset).c_str()) + ", " + message + ");\n";
        }

        output += "};\n} // namespace Hyperion\n";

        if (cls.condition.Any())
        {
            output += "#endif // " + cls.condition + "\n";
        }

        writer.WriteString(output);
    }

    return {};
}

String CBindingGenerator::FormatBindingManifest(Array<ThunkBindingRecord> records, const BindingLayoutSet& layouts)
{
    std::sort(records.Begin(), records.End(), [](const ThunkBindingRecord& a, const ThunkBindingRecord& b)
        {
            return a.symbol < b.symbol;
        });

    String result = "{\n  \"abiVersion\": 1,\n  \"bindings\": [\n";

    for (size_t i = 0; i < records.Size(); ++i)
    {
        const ThunkBindingRecord& record = records[i];

        Array<String> params;

        for (const ThunkBindingValue& param : record.params)
        {
            params.PushBack(FormatBindingManifestValue(param, true));
        }

        result += "    { \"symbol\": \"" + record.symbol + "\"";
        result += ", \"class\": \"" + record.className + "\"";
        result += ", \"member\": \"" + record.memberName + "\"";
        result += ", \"kind\": \"" + record.kind + "\"";
        result += ", \"condition\": \"" + record.condition + "\"";
        result += String(", \"static\": ") + (record.isStatic ? "true" : "false");
        result += ", \"params\": [" + String::Join(params, ", ") + "]";
        result += ", \"return\": " + FormatBindingManifestValue(record.returnValue, false);

        if (record.hasResultValue)
        {
            result += ", \"resultValue\": " + FormatBindingManifestValue(record.resultValue, false);
        }

        result += " }";
        result += i + 1 < records.Size() ? ",\n" : "\n";
    }

    result += "  ],\n  \"structs\": [\n";

    const Array<const BindingLayout*> generatedLayouts = layouts.GetGeneratedLayouts();

    for (size_t i = 0; i < generatedLayouts.Size(); ++i)
    {
        const BindingLayout& layout = *generatedLayouts[i];

        Array<String> fields;

        for (const BindingLayoutField& field : layout.fields)
        {
            fields.PushBack("{ \"name\": \"" + field.name + "\", \"type\": \"" + field.cType + "\", \"count\": " + String(std::to_string(field.count).c_str())
                + ", \"offset\": " + String(std::to_string(field.offset).c_str()) + " }");
        }

        result += "    { \"name\": \"" + layout.name + "\", \"size\": " + String(std::to_string(layout.size).c_str())
            + ", \"align\": " + String(std::to_string(layout.align).c_str()) + ", \"fields\": [" + String::Join(fields, ", ") + "] }";
        result += i + 1 < generatedLayouts.Size() ? ",\n" : "\n";
    }

    result += "  ]\n}\n";

    return result;
}

Result CBindingGenerator::EmitThunks(const Analyzer& analyzer, const Module& mod, const Set<String>& allHandleNames, ByteWriter& writer, Array<ThunkBindingRecord>* outRecords) const
{
    bool openedBlock = false;

    auto ensureBindingHeader = [&]()
    {
        if (!openedBlock)
        {
            writer.WriteString("\n#include <Core/Scripting/Strata/ThunkDrawer.hpp>\n");
            writer.WriteString("#include <Core/Scripting/Strata/StrataMarshal.hpp>\n\n");

            openedBlock = true;
        }
    };

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        if ((cls.type != ClassDefinitionType::Class && cls.type != ClassDefinitionType::Struct
                && cls.type != ClassDefinitionType::Enum)
            || !IsStrataScriptable(analyzer, cls))
        {
            continue;
        }

        Set<String> emittedExternNames;
        String classThunks;
        String classRegistrations;

        // Thunks are static members of a TClassStaticInit specialization: reflected classes befriend that template,
        // so a thunk can reach a non-public HYP_METHOD the same way the class's reflection data does.
        const String accessTag = HYP_FORMAT("CBindings_{}", cls.name);

        auto appendThunk = [&](const String& condition, const String& definition, const String& registerVariable, const String& symbol)
        {
            static const String externPrefix = "extern \"C\" ";

            const String member = "    static " + String(definition.Substr(externPrefix.Size(), definition.Size()));
            const String registration = HYP_FORMAT("static const bool {} = ::Hyperion::Strata::ThunkDrawer::Register(\"{}\"_sh, reinterpret_cast<void*>(&::Hyperion::TClassStaticInit<::Hyperion::{}>::{}));\n",
                registerVariable, symbol, accessTag, symbol);

            if (condition.Any())
            {
                classThunks += HYP_FORMAT("#if {}\n", condition) + member + HYP_FORMAT("#endif // {}\n", condition);
                classRegistrations += HYP_FORMAT("#if {}\n", condition) + registration + HYP_FORMAT("#endif // {}\n", condition);
            }
            else
            {
                classThunks += member;
                classRegistrations += registration;
            }
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

            // Predicate must match EmitMethods exactly.
            StrataTypeMapping returnTypeMapping;

            if (TResult<StrataTypeMapping> res = MapToStrataType(analyzer, functionType->returnType); res.HasError())
            {
                continue;
            }
            else
            {
                returnTypeMapping = res.GetValue();
            }

            if ((returnTypeMapping.isHandle || returnTypeMapping.isEnum)
                && !allHandleNames.Contains(returnTypeMapping.typeName))
            {
                continue;
            }

            Array<String> sigParams; // "Type name" for the thunk signature
            Array<String> callArgs;  // "name" for the forwarded call
            Array<ThunkBindingValue> recordParams;
            bool paramsOk = true;

            for (size_t j = 0; j < functionType->parameters.Size(); ++j)
            {
                const ASTMemberDecl* parameter = functionType->parameters[j];

                const String paramName = parameter->name.Any() ? parameter->name : HYP_FORMAT("arg{}", j);

                TResult<StrataTypeMapping> paramRes = BuildThunkInputParam(analyzer, allHandleNames, parameter->type.Get(), paramName, sigParams, callArgs);

                if (paramRes.HasError())
                {
                    paramsOk = false;
                    break;
                }

                recordParams.PushBack({ paramName, ThunkTypeName(paramRes.GetValue()), ThunkInputConvention(paramRes.GetValue()) });
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

            // Chained setters emit void-shaped; the fluent return is discarded.
            const bool propertySetterIdiom = IsPropertySetterIdiom(cls, member);
            const bool returnsViaOutParam = !propertySetterIdiom && !returnTypeMapping.isResult
                && (returnTypeMapping.isStructValue || returnTypeMapping.isArray || returnTypeMapping.isString || returnTypeMapping.isVector);

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

            if (returnTypeMapping.isResult && !propertySetterIdiom)
            {
                if (returnTypeMapping.hasResultValue)
                {
                    if (returnTypeMapping.isHandleWrapper)
                    {
                        allSigParams.PushBack(HYP_FORMAT("{}** outValue", returnTypeMapping.CxxTypeName()));
                    }
                    else if (returnTypeMapping.isString)
                    {
                        allSigParams.PushBack("::Hyperion::Strata::SString* outValue");
                    }
                    else if (returnTypeMapping.isStructValue || returnTypeMapping.isVector)
                    {
                        allSigParams.PushBack(HYP_FORMAT("{}* outValue", returnTypeMapping.CxxTypeName()));
                    }
                    else
                    {
                        // scalar or enum: spell it as the C++ signature does
                        allSigParams.PushBack(functionType->returnType->templateArguments[0]->type->Format() + "* outValue");
                    }
                }

                allSigParams.PushBack("::Hyperion::Strata::SString* outError");
            }
            else if (returnTypeMapping.isArray)
            {
                // Reference-stripping like params: getters return `const Array<T>&`.
                const ASTType* unwrappedReturnType = (functionType->returnType->isLvalueReference || functionType->returnType->isRvalueReference)
                    ? functionType->returnType->refTo.Get()
                    : functionType->returnType.Get();

                const String returnElementCxxType = unwrappedReturnType->templateArguments[0]->type->Format();

                allSigParams.PushBack(HYP_FORMAT("::Hyperion::Strata::SArray<{}>* outReturn", returnElementCxxType));
            }
            else if (returnTypeMapping.isString)
            {
                // `string` is a {ptr, len} fat; the host writes it via SetReturnString.
                allSigParams.PushBack("::Hyperion::Strata::SString* outReturn");
            }
            else if (returnsViaOutParam)
            {
                allSigParams.PushBack(HYP_FORMAT("{}* outReturn", returnTypeMapping.CxxTypeName()));
            }

            const String sigParamsString = allSigParams.Any() ? String::Join(allSigParams, ", ") : String("");
            const String callArgsString = callArgs.Any() ? String::Join(callArgs, ", ") : String("");

            const String callExpr = isStatic
                ? HYP_FORMAT("{}::{}({})", cls.name, member.name, callArgsString)
                : HYP_FORMAT("self->{}({})", member.name, callArgsString);

            // Wrap the thunk + registrar in the method's preprocessor condition.
            const StrataTypeMapping thunkReturnMapping = propertySetterIdiom
                ? StrataTypeMapping { "void" }
                : returnTypeMapping;

            const bool retainedReturn = !propertySetterIdiom && !returnTypeMapping.isResult && IsRetainedHandleReturn(returnTypeMapping, functionType->returnType.Get());

            String methodOutput = FormatThunkDefinition(externName, thunkReturnMapping,
                propertySetterIdiom ? nullptr : functionType->returnType.Get(), sigParamsString, callExpr, retainedReturn);

            // Register for static init.

            if (outRecords != nullptr)
            {
                ThunkBindingRecord record;
                record.symbol = externName;
                record.className = cls.name;
                record.memberName = managedName;
                record.kind = "method";
                record.condition = cls.condition.Any() && member.condition.Any() ? cls.condition + " && " + member.condition : (cls.condition.Any() ? cls.condition : member.condition);
                record.isStatic = isStatic;
                record.params = std::move(recordParams);
                record.returnValue = { String::empty, ThunkTypeName(thunkReturnMapping), retainedReturn ? String("handle_retained") : ThunkReturnConvention(thunkReturnMapping) };

                if (thunkReturnMapping.isResult)
                {
                    record.returnValue.type = "Result";

                    if (thunkReturnMapping.hasResultValue)
                    {
                        StrataTypeMapping valueMapping = thunkReturnMapping;
                        valueMapping.isResult = false;

                        record.hasResultValue = true;
                        record.resultValue = { "outValue", ThunkTypeName(valueMapping),
                            valueMapping.isHandleWrapper ? String("handle_retained") : ThunkReturnConvention(valueMapping) };
                    }
                }

                outRecords->PushBack(std::move(record));
            }

            appendThunk(member.condition, methodOutput, HYP_FORMAT("s_cBinding_{}_{}", cls.name, managedName), externName);
        }

        // Property accessor thunks; reused externs need none.
        {
            for (const ResolvedStrataProperty& prop : CollectImplProperties(analyzer, cls, allHandleNames))
            {
                PropertyAccessorBinding binding;

                if (!ResolvePropertyAccessorBinding(analyzer, cls, prop, allHandleNames, emittedExternNames, binding))
                {
                    continue;
                }

                const String selfParam = HYP_FORMAT("{}* self", cls.name);

                if (binding.getterSynthesized)
                {
                    const String getterCall = prop.getter->cxxType->isFunction
                        ? HYP_FORMAT("self->{}()", prop.getter->name)
                        : HYP_FORMAT("self->{}", prop.getter->name);

                    // String getters write the {ptr, len} fat through an out-param.
                    // -- Why? They don't have to, anymore --
                    // They don't have to per se; but because strata extern char* returns do NOT hand off ownership,
                    // we do it this way so ownership is created on the C++ side and strata takes it over.
                    Array<String> getterSigParams;
                    getterSigParams.PushBack(selfParam);

                    if (prop.mapping.isString)
                    {
                        getterSigParams.PushBack("::Hyperion::Strata::SString* outReturn");
                    }
                    else if (prop.mapping.isVector)
                    {
                        getterSigParams.PushBack(HYP_FORMAT("{}* outReturn", prop.mapping.CxxTypeName()));
                    }

                    String getterThunk = FormatThunkDefinition(prop.getterSymbol, prop.mapping, prop.getterValueType,
                        String::Join(getterSigParams, ", "), getterCall);

                    if (outRecords != nullptr)
                    {
                        ThunkBindingRecord record;
                        record.symbol = prop.getterSymbol;
                        record.className = cls.name;
                        record.memberName = prop.name;
                        record.kind = "getter";
                        record.condition = cls.condition;
                        record.returnValue = { String::empty, ThunkTypeName(prop.mapping), ThunkReturnConvention(prop.mapping) };

                        outRecords->PushBack(std::move(record));
                    }

                    appendThunk(PropertyAccessorCondition(prop, prop.getter), getterThunk,
                        HYP_FORMAT("s_cPropertyBinding_{}_get_{}", cls.name, prop.name), prop.getterSymbol);

                    emittedExternNames.Insert(binding.getterSymbol);
                }

                if (binding.setterSynthesized)
                {
                    Array<String> sigParams;
                    Array<String> callArgs;
                    sigParams.PushBack(selfParam);

                    TResult<StrataTypeMapping> valueRes = BuildThunkInputParam(analyzer, allHandleNames, prop.setterValueType, "value", sigParams, callArgs);

                    if (!valueRes.HasError())
                    {
                        const String setterCall = prop.setter->cxxType->isFunction
                            ? HYP_FORMAT("self->{}({})", prop.setter->name, String::Join(callArgs, ", "))
                            : HYP_FORMAT("self->{} = {}", prop.setter->name, callArgs[0]);

                        String setterThunk = FormatThunkDefinition(prop.setterSymbol, StrataTypeMapping { "void" }, nullptr, String::Join(sigParams, ", "), setterCall);

                        if (outRecords != nullptr)
                        {
                            ThunkBindingRecord record;
                            record.symbol = prop.setterSymbol;
                            record.className = cls.name;
                            record.memberName = prop.name;
                            record.kind = "setter";
                            record.condition = cls.condition;
                            record.params.PushBack({ "value", ThunkTypeName(valueRes.GetValue()), ThunkInputConvention(valueRes.GetValue()) });
                            record.returnValue = { String::empty, "void", "void" };

                            outRecords->PushBack(std::move(record));
                        }

                        appendThunk(PropertyAccessorCondition(prop, prop.setter), setterThunk,
                            HYP_FORMAT("s_cPropertyBinding_{}_set_{}", cls.name, prop.name), prop.setterSymbol);

                        emittedExternNames.Insert(binding.setterSymbol);
                    }
                }
            }
        }

        if (!classThunks.Any())
        {
            continue;
        }

        ensureBindingHeader();

        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#if {}\n\n", cls.condition));
        }

        writer.WriteString(String("namespace Hyperion {\nstruct ") + accessTag + ";\ntemplate <>\nclass TClassStaticInit<" + accessTag + "> final\n{\npublic:\n");
        writer.WriteString(classThunks);
        writer.WriteString("};\n} // namespace Hyperion\n\n");
        writer.WriteString(classRegistrations);
        writer.WriteString("\n");

        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#endif // {}\n\n", cls.condition));
        }
    }

    return {};
}

} // namespace CodeGen
} // namespace Hyperion
