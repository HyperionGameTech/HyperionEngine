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
#include <utility>

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

    // "_Impl"-suffixed methods are internal default-implementation helpers, not meant to be called directly from Strata.
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

// A reflected property resolved to its Strata representation. Accessors are
// synthesized extern symbols (e.g. Camera_Get_FOV), backed by thunks in
// EmitThunks; either side may be absent.
// also resolves syntheti properties.
struct ResolvedStrataProperty
{
    String name;
    StrataTypeMapping mapping;
    const MemberDef* getter = nullptr;
    const MemberDef* setter = nullptr;
    const ASTType* getterValueType = nullptr; // getter return / referenced field type
    const ASTType* setterValueType = nullptr; // setter value param / referenced field type
    String getterSymbol;
    String setterSymbol;
    String condition; // property-level preprocessor condition (macro form)
};

struct StrataAccessorBinding
{
    const MemberDef* member = nullptr;
    const ASTType* valueType = nullptr;
    StrataTypeMapping mapping;
};

// Resolves one accessor (getter method/field, or setter method/field) to the
// Strata representation of the property value it produces/consumes.
TResult<StrataAccessorBinding> ResolveStrataAccessorBinding(const Analyzer& analyzer, const String& propertyName,
    const MemberDef& refMember, bool isSetter, const Set<String>& allHandleNames)
{
    if (!refMember.cxxType || !MemberIsStrataScriptable(refMember))
    {
        return HYP_MAKE_ERROR(Error, "Property '{}' accessor '{}' could not be resolved", propertyName, refMember.name);
    }

    const ASTType* valueType;

    if (refMember.cxxType->isFunction)
    {
        const ASTFunctionType* functionType = dynamic_cast<const ASTFunctionType*>(refMember.cxxType.Get());

        if (!functionType)
        {
            return HYP_MAKE_ERROR(Error, "Property '{}' accessor '{}' has an unsupported function type", propertyName, refMember.name);
        }

        if (isSetter)
        {
            if (functionType->parameters.Size() != 1)
            {
                return HYP_MAKE_ERROR(Error, "Property '{}' setter '{}' must take exactly one parameter", propertyName, refMember.name);
            }

            valueType = functionType->parameters[0]->type.Get();
        }
        else
        {
            if (functionType->parameters.Size() != 0)
            {
                return HYP_MAKE_ERROR(Error, "Property '{}' getter '{}' must not take parameters", propertyName, refMember.name);
            }

            valueType = functionType->returnType.Get();
        }
    }
    else
    {
        if (refMember.cxxType->isStatic)
        {
            return HYP_MAKE_ERROR(Error, "Property '{}' cannot use static member '{}'", propertyName, refMember.name);
        }

        valueType = refMember.cxxType.Get();
    }

    if (valueType == nullptr || valueType->IsVoid())
    {
        return HYP_MAKE_ERROR(Error, "Property '{}' has no value type", propertyName);
    }

    TResult<StrataTypeMapping> mapRes = MapToStrataType(analyzer, valueType);

    if (mapRes.HasError())
    {
        return mapRes.GetError();
    }

    StrataTypeMapping mapping = mapRes.GetValue();

    // A property is a single value: aggregates and fat arrays cannot cross
    // the extern boundary as a return value.
    if (mapping.isStructValue || mapping.isArray)
    {
        return HYP_MAKE_ERROR(Error, "Property '{}' type '{}' is not representable as a Strata property", propertyName, mapping.typeName);
    }

    if (mapping.isHandle && !allHandleNames.Contains(mapping.typeName))
    {
        return HYP_MAKE_ERROR(Error, "Property '{}' references undeclared handle type '{}'", propertyName, mapping.typeName);
    }

    return StrataAccessorBinding { &refMember, valueType, std::move(mapping) };
}

// Builds the resolved property from already-resolved accessors. A missing or
// unresolvable setter yields a read-only property; mismatched accessor types
// skip the property entirely.
TResult<ResolvedStrataProperty> BuildResolvedStrataProperty(const Analyzer& analyzer, const ClassDefinition& cls,
    const String& name, const MemberDef* getter, const MemberDef* setter, const String& condition,
    const Set<String>& allHandleNames)
{
    if (!getter)
    {
        return HYP_MAKE_ERROR(Error, "Property '{}' has no getter", name);
    }

    TResult<StrataAccessorBinding> getterRes = ResolveStrataAccessorBinding(analyzer, name, *getter, false, allHandleNames);

    if (getterRes.HasError())
    {
        return getterRes.GetError();
    }

    ResolvedStrataProperty result;
    result.name = name;
    result.condition = condition;
    result.getter = getterRes.GetValue().member;
    result.getterValueType = getterRes.GetValue().valueType;
    result.mapping = getterRes.GetValue().mapping;
    result.getterSymbol = HYP_FORMAT("{}_Get_{}", cls.name, result.name);

    if (setter)
    {
        TResult<StrataAccessorBinding> setterRes = ResolveStrataAccessorBinding(analyzer, name, *setter, true, allHandleNames);

        if (!setterRes.HasError())
        {
            if (setterRes.GetValue().mapping.typeName != result.mapping.typeName)
            {
                return HYP_MAKE_ERROR(Error, "Property '{}' getter and setter types differ ('{}' vs '{}')",
                    name, result.mapping.typeName, setterRes.GetValue().mapping.typeName);
            }

            result.setter = setterRes.GetValue().member;
            result.setterValueType = setterRes.GetValue().valueType;
            result.setterSymbol = HYP_FORMAT("{}_Set_{}", cls.name, result.name);
        }
    }

    return result;
}

// Resolves a HYP_PROPERTY member ("&Class::Member" references; the property
// name was popped by the analyzer). A failing setter reference yields a
// read-only property instead.
TResult<ResolvedStrataProperty> ResolveStrataProperty(const Analyzer& analyzer, const ClassDefinition& cls,
    const MemberDef& member, const Set<String>& allHandleNames)
{
    Array<const String*> memberRefs;

    for (const Pair<String, ClassAttributeValue>& attribute : member.attributes)
    {
        if (attribute.first.StartsWith("&"))
        {
            memberRefs.PushBack(&attribute.first);
        }
    }

    if (memberRefs.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Property '{}' has no getter reference", member.name);
    }

    auto findRefMember = [&cls](const String& attrValue) -> const MemberDef*
    {
        const String refPath = String(attrValue.Substr(1)).Trimmed();

        const size_t colonPos = refPath.FindLastIndex(UTF8StringView("::"));

        if (colonPos == String::NotFound)
        {
            return nullptr;
        }

        const String refMemberName = refPath.Substr(colonPos + 2);

        for (const MemberDef& clsMember : cls.members)
        {
            if (clsMember.name == refMemberName)
            {
                return &clsMember;
            }
        }

        return nullptr;
    };

    const MemberDef* getter = findRefMember(*memberRefs[0]);

    if (!getter)
    {
        return HYP_MAKE_ERROR(Error, "Property '{}' getter reference '{}' could not be resolved", member.name, *memberRefs[0]);
    }

    const MemberDef* setter = nullptr;

    if (memberRefs.Size() >= 2)
    {
        setter = findRefMember(*memberRefs[1]);
    }

    return BuildResolvedStrataProperty(analyzer, cls, member.friendlyName, getter, setter, member.condition, allHandleNames);
}

// Collects all Strata-representable properties of a class, in both engine
// forms: HYP_PROPERTY macro members, then HYP_METHOD(Property = "...") pairs.
Array<ResolvedStrataProperty> CollectImplProperties(const Analyzer& analyzer, const ClassDefinition& cls,
    const Set<String>& allHandleNames)
{
    Array<ResolvedStrataProperty> properties;

    for (const MemberDef& member : cls.members)
    {
        if (member.type != MemberType::Property || !MemberIsStrataScriptable(member))
        {
            continue;
        }

        if (TResult<ResolvedStrataProperty> res = ResolveStrataProperty(analyzer, cls, member, allHandleNames); !res.HasError())
        {
            properties.PushBack(std::move(res.GetValue()));
        }
    }

    // Getter/setter pairs linked by their shared Property attribute value.
    Map<String, Pair<const MemberDef*, const MemberDef*>> paired; // name -> {getter, setter}
    Array<String> order;

    for (const MemberDef& member : cls.members)
    {
        if (member.type != MemberType::Method || !MemberIsStrataScriptable(member)
            || !member.cxxType || !member.cxxType->isFunction)
        {
            continue;
        }

        const ClassAttributeValue& attr = member.GetAttribute(Attributes::g_attrProperty);

        if (!attr.IsValid() || !attr.IsString())
        {
            continue;
        }

        const String propertyName = attr.GetString();

        auto it = paired.Find(propertyName);

        if (it == paired.End())
        {
            it = paired.Set(propertyName, { nullptr, nullptr }).first;
            order.PushBack(propertyName);
        }

        const ASTFunctionType* functionType = dynamic_cast<const ASTFunctionType*>(member.cxxType.Get());
        const bool paramsEmpty = functionType != nullptr && functionType->parameters.Size() == 0;

        if (paramsEmpty && it->second.first == nullptr)
        {
            it->second.first = &member;
        }
        else if (!paramsEmpty && it->second.second == nullptr)
        {
            it->second.second = &member;
        }
    }

    for (const String& propertyName : order)
    {
        auto it = paired.Find(propertyName);

        if (it == paired.End())
        {
            continue;
        }

        if (TResult<ResolvedStrataProperty> res = BuildResolvedStrataProperty(analyzer, cls, propertyName,
                it->second.first, it->second.second, String::empty, allHandleNames); !res.HasError())
        {
            properties.PushBack(std::move(res.GetValue()));
        }
    }

    return properties;
}

// The property-level condition plus the accessor's own condition must both hold.
String PropertyAccessorCondition(const ResolvedStrataProperty& prop, const MemberDef* accessor)
{
    if (prop.condition.Any() && accessor->condition.Any())
    {
        return HYP_FORMAT("{} && {}", prop.condition, accessor->condition);
    }

    return prop.condition.Any() ? prop.condition : accessor->condition;
}

// Maps one value crossing the extern boundary (method param or property setter
// value) from its Strata representation to the C++ call form: appends the thunk
// signature param and the forwarded call argument.
TResult<StrataTypeMapping> BuildThunkInputParam(const Analyzer& analyzer, const Set<String>& allHandleNames,
    const ASTType* paramType, const String& paramName, Array<String>& sigParams, Array<String>& callArgs)
{
    TResult<StrataTypeMapping> paramRes = MapToStrataType(analyzer, paramType);

    if (paramRes.HasError())
    {
        return paramRes;
    }

    const StrataTypeMapping paramTypeMapping = paramRes.GetValue();

    if (paramTypeMapping.isHandle && !allHandleNames.Contains(paramTypeMapping.typeName))
    {
        return HYP_MAKE_ERROR(Error, "Handle type '{}' is not a declared Strata handle", paramTypeMapping.typeName);
    }

    // The parameter type with any top-level reference stripped: Strata's
    // string/array/vector values don't cross the boundary as C++
    // references, even though the C++ signature conventionally takes
    // them by const&.
    const ASTType* unwrappedParamType = (paramType->isLvalueReference || paramType->isRvalueReference)
        ? paramType->refTo.Get()
        : paramType;

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

// Formats the `extern "C"` thunk definition forwarding callExpr, returning
// returnTypeMapping's representation at the host boundary.
String FormatThunkDefinition(const String& externName, const StrataTypeMapping& returnTypeMapping,
    const ASTType* cxxReturnType, const String& sigParamsString, const String& callExpr)
{
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
    else if (cxxReturnType == nullptr || cxxReturnType->IsVoid())
    {
        methodOutput += HYP_FORMAT("extern \"C\" void {}({})", externName, sigParamsString);
        methodOutput += " { ";
        methodOutput += callExpr;
        methodOutput += "; }\n";
    }
    else if (returnTypeMapping.isVector)
    {
        // float3/float4 cross the extern "C" boundary as a raw SIMD
        // register (stratac lowers them to __m128/float32x4_t), not a
        // Vec3f/Vec4f struct by value - convert explicitly via the
        // overloaded ToSimdVector (picks Vec3f/Vec4f by callExpr's type).
        methodOutput += HYP_FORMAT("extern \"C\" ::Hyperion::Strata::SimdVector {}({})", externName, sigParamsString);
        methodOutput += " { return ::Hyperion::Strata::ToSimdVector(";
        methodOutput += callExpr;
        methodOutput += "); }\n";
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

} // anonymous namespace

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

        // Classes are Strata handles and take impl blocks with methods and
        // C#-style properties. Structs have no impl form; their methods stay
        // flat extern free functions taking the struct as the first parameter.
        const bool isHandleType = cls.type == ClassDefinitionType::Class;

        String classOutput;
        bool emittedAnyForClass = false;

        // Track emitted extern symbol names (fully qualified) so C++ overloads
        // (same method name, different params) and property accessors don't
        // produce duplicate Strata symbols, which would fail to compile.
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

            if (isHandleType)
            {
                classOutput += HYP_FORMAT("impl {}", cls.name) + " {\n";
            }

            emittedAnyForClass = true;
        };

        for (size_t i = 0; i < cls.members.Size(); ++i)
        {
            const MemberDef& member = cls.members[i];

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

            // Strata cannot return aggregates (structs, fat arrays) across the
            // ABI boundary; write those through an out (ref) param instead.
            // `string` and float3/float4 are single-value/core types and
            // return directly.
            const bool returnsViaOutParam = returnTypeMapping.isStructValue || returnTypeMapping.isArray;
            String strataReturnType = returnTypeMapping.typeName;

            if (returnsViaOutParam)
            {
                strataReturnType = "void";
                paramDecls.PushBack(HYP_FORMAT("ref {} outReturn", returnTypeMapping.typeName));
            }

            const String managedName = ResolveManagedName(member);
            // The actual extern symbol: impl-block methods are renamed by the
            // Strata parser (`Handle_Method`), structs keep flat extern names.
            const String externName = HYP_FORMAT("{}_{}", cls.name, managedName);

            // Skip C++ overloads that would collide with an already-emitted symbol.
            if (emittedExternNames.Contains(externName))
            {
                continue;
            }

            ensureClassHeader();

            emittedExternNames.Insert(externName);

            const String paramsString = paramDecls.Any() ? String::Join(paramDecls, ", ") : String("");

            // Strata has no preprocessor; record a method-level condition (e.g.
            // EditorOnly) as a comment. The matching C++ thunk is #if-guarded, so
            // where the condition is false the extern simply won't be registered.
            if (member.condition.Any())
            {
                classOutput += HYP_FORMAT("    // requires: {}\n", member.condition);
            }

            classOutput += HYP_FORMAT("{}extern {} {}({});\n",
                isHandleType ? "    " : "", strataReturnType, isHandleType ? managedName : externName, paramsString);
        }

        // Properties are an impl-block feature: handles only. Both engine forms
        // (HYP_PROPERTY macro members and HYP_METHOD(Property = "...") pairs)
        // are collected by CollectImplProperties.
        if (isHandleType)
        {
            for (const ResolvedStrataProperty& prop : CollectImplProperties(analyzer, cls, allHandleNames))
            {
                if (emittedExternNames.Contains(prop.getterSymbol)
                    || (prop.setterSymbol.Any() && emittedExternNames.Contains(prop.setterSymbol)))
                {
                    continue;
                }

                ensureClassHeader();

                // Strata has no preprocessor; record a property-level condition
                // (e.g. EditorOnly) as a comment. The matching accessor thunks
                // are #if-guarded, so where the condition is false the extern
                // simply won't be registered.
                if (prop.condition.Any())
                {
                    classOutput += HYP_FORMAT("    // requires: {}\n", prop.condition);
                }

                String accessors;

                if (prop.getterSymbol.Any())
                {
                    accessors += HYP_FORMAT("get = {};", prop.getterSymbol);
                    emittedExternNames.Insert(prop.getterSymbol);
                }

                if (prop.setterSymbol.Any())
                {
                    accessors += HYP_FORMAT("{}set = {};", accessors.Any() ? " " : "", prop.setterSymbol);
                    emittedExternNames.Insert(prop.setterSymbol);
                }

                classOutput += HYP_FORMAT("    property {} {} ", prop.mapping.typeName, prop.name) + "{ " + accessors + " }\n";
            }
        }

        if (emittedAnyForClass)
        {
            if (isHandleType)
            {
                classOutput += "}\n";
            }

            writer.WriteString(classOutput);
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

                const String paramName = parameter->name.Any() ? parameter->name : HYP_FORMAT("arg{}", j);

                if (TResult<StrataTypeMapping> paramRes = BuildThunkInputParam(analyzer, allHandleNames, parameter->type.Get(), paramName, sigParams, callArgs); paramRes.HasError())
                {
                    paramsOk = false;

                    break;
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

            const bool returnsViaOutParam = returnTypeMapping.isStructValue || returnTypeMapping.isArray;

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
            else if (returnsViaOutParam)
            {
                // Qualified for the same reason as the struct-value param case above.
                allSigParams.PushBack(HYP_FORMAT("{}* outReturn", returnTypeMapping.CxxTypeName()));
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
            String methodOutput = FormatThunkDefinition(externName, returnTypeMapping, functionType->returnType.Get(), sigParamsString, callExpr);

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

        // Property accessor thunks (impl blocks are handles-only, like the
        // .strata side emitted by EmitMethods).
        if (cls.type == ClassDefinitionType::Class)
        {
            for (const ResolvedStrataProperty& prop : CollectImplProperties(analyzer, cls, allHandleNames))
            {
                // Skip the whole property if either accessor symbol collides,
                // matching EmitMethods' skip decision.
                if (emittedExternNames.Contains(prop.getterSymbol)
                    || (prop.setterSymbol.Any() && emittedExternNames.Contains(prop.setterSymbol)))
                {
                    continue;
                }

                const String selfParam = HYP_FORMAT("{}* self", cls.name);

                if (prop.getter)
                {
                    const String getterCall = prop.getter->cxxType->isFunction
                        ? HYP_FORMAT("self->{}()", prop.getter->name)
                        : HYP_FORMAT("self->{}", prop.getter->name);

                    String getterThunk = FormatThunkDefinition(prop.getterSymbol, prop.mapping, prop.getterValueType, selfParam, getterCall);
                    getterThunk += HYP_FORMAT("static const bool s_strataPropertyBinding_{}_get_{} = ::Hyperion::Strata::ThunkDrawer::Register(\"{}\"_sh, reinterpret_cast<void*>(&{}));\n",
                        cls.name, prop.name, prop.getterSymbol, prop.getterSymbol);

                    const String condition = PropertyAccessorCondition(prop, prop.getter);

                    if (condition.Any())
                    {
                        classThunks += HYP_FORMAT("#if {}\n", condition);
                    }

                    classThunks += getterThunk;

                    if (condition.Any())
                    {
                        classThunks += HYP_FORMAT("#endif // {}\n", condition);
                    }

                    classThunks += "\n";

                    emittedExternNames.Insert(prop.getterSymbol);
                }

                if (prop.setter)
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
                        setterThunk += HYP_FORMAT("static const bool s_strataPropertyBinding_{}_set_{} = ::Hyperion::Strata::ThunkDrawer::Register(\"{}\"_sh, reinterpret_cast<void*>(&{}));\n",
                            cls.name, prop.name, prop.setterSymbol, prop.setterSymbol);

                        const String condition = PropertyAccessorCondition(prop, prop.setter);

                        if (condition.Any())
                        {
                            classThunks += HYP_FORMAT("#if {}\n", condition);
                        }

                        classThunks += setterThunk;

                        if (condition.Any())
                        {
                            classThunks += HYP_FORMAT("#endif // {}\n", condition);
                        }

                        classThunks += "\n";

                        emittedExternNames.Insert(prop.setterSymbol);
                    }
                }
            }
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
