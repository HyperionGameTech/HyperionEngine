/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/BindingsShared.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <Util/Util.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <utility>

namespace Hyperion {
namespace CodeGen {
namespace BindingsShared {

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

    // "_Impl"-suffixed methods are internal defaults, not callable from Strata.
    if (member.name.EndsWith("_Impl"))
    {
        return false;
    }

    // Operator overloads have no valid Strata identifier spelling.
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

// Resolves one property accessor (getter or setter) to its Strata value type.
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

        if (refMember.cxxType->isStatic)
        {
            return HYP_MAKE_ERROR(Error, "Property '{}' accessor '{}' must not be static", propertyName, refMember.name);
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

    // Struct values and arrays cannot be single-value properties.
    if (mapping.isStructValue || mapping.isArray)
    {
        return HYP_MAKE_ERROR(Error, "Property '{}' type '{}' is not representable as a Strata property", propertyName, mapping.typeName);
    }

    if ((mapping.isHandle || mapping.isEnum) && !allHandleNames.Contains(mapping.typeName))
    {
        return HYP_MAKE_ERROR(Error, "Property '{}' references undeclared {} type '{}'", propertyName,
            mapping.isEnum ? "enum" : "handle", mapping.typeName);
    }

    return StrataAccessorBinding { &refMember, valueType, std::move(mapping) };
}

// Builds a resolved property from its accessors. A missing setter yields a
// read-only property.
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

// Resolves a HYP_PROPERTY member ("&Class::Member" references).
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

// Collects a class's Strata properties: HYP_PROPERTY members, then
// HYP_METHOD(Property = "...") pairs.
Array<ResolvedStrataProperty> CollectImplProperties(const Analyzer& analyzer, const ClassDefinition& cls,
    const Set<String>& allHandleNames)
{
    // Enums have no instance properties.
    if (cls.type == ClassDefinitionType::Enum)
    {
        return {};
    }

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

    // Getter/setter pairs linked by a shared Property attribute.
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

// Both the property and accessor conditions must hold.
String PropertyAccessorCondition(const ResolvedStrataProperty& prop, const MemberDef* accessor)
{
    if (prop.condition.Any() && accessor->condition.Any())
    {
        return HYP_FORMAT("{} && {}", prop.condition, accessor->condition);
    }

    return prop.condition.Any() ? prop.condition : accessor->condition;
}

// Only the first scriptable member with a managed name gets an extern symbol.
bool IsEmittedOverload(const ClassDefinition& cls, const MemberDef& member)
{
    const String managedName = ResolveManagedName(member);

    for (const MemberDef& clsMember : cls.members)
    {
        if (&clsMember == &member)
        {
            return true;
        }

        if (MemberIsStrataScriptable(clsMember) && ResolveManagedName(clsMember) == managedName)
        {
            return false;
        }
    }

    return false;
}

// Is it shaped like a setter?
bool IsPropertySetterIdiom(const ClassDefinition& cls, const MemberDef& member)
{
    if (!member.cxxType || !member.cxxType->isFunction || member.cxxType->isStatic)
    {
        return false;
    }

    const ClassAttributeValue& attr = member.GetAttribute(Attributes::g_attrProperty);

    if (!attr.IsValid() || !attr.IsString())
    {
        return false;
    }

    const ASTFunctionType* functionType = dynamic_cast<const ASTFunctionType*>(member.cxxType.Get());

    if (!functionType || functionType->parameters.Size() != 1)
    {
        return false;
    }

    if (functionType->returnType->IsVoid())
    {
        return false; // already void-shaped
    }

    const ASTType* unwrappedReturnType = (functionType->returnType->isLvalueReference || functionType->returnType->isRvalueReference)
        ? functionType->returnType->refTo.Get()
        : functionType->returnType.Get();

    return unwrappedReturnType != nullptr
        && unwrappedReturnType->typeName.HasValue()
        && unwrappedReturnType->typeName->ToString(/* includeNamespace */ false) == cls.name;
}

// The method's own extern symbol when it already matches the property accessor
// shape; empty when a dedicated thunk is needed.
String ResolveExistingAccessorSymbol(const Analyzer& analyzer, const ClassDefinition& cls,
    const MemberDef& accessor, const StrataTypeMapping& propMapping, bool isSetter,
    const Set<String>& allHandleNames)
{
    if (!accessor.cxxType || !accessor.cxxType->isFunction || accessor.cxxType->isStatic || !IsEmittedOverload(cls, accessor))
    {
        return String::empty;
    }

    const ASTFunctionType* functionType = dynamic_cast<const ASTFunctionType*>(accessor.cxxType.Get());

    if (!functionType)
    {
        return String::empty;
    }

    if (isSetter)
    {
        if (functionType->parameters.Size() != 1)
        {
            return String::empty;
        }

        // Void, or the chained-setter idiom (whose binding emits void-shaped).
        if (!functionType->returnType->IsVoid() && !IsPropertySetterIdiom(cls, accessor))
        {
            return String::empty;
        }

        TResult<StrataTypeMapping> paramRes = MapToStrataType(analyzer, functionType->parameters[0]->type.Get());

        if (paramRes.HasError() || paramRes.GetValue().typeName != propMapping.typeName)
        {
            return String::empty;
        }
    }
    else
    {
        if (functionType->parameters.Size() != 0)
        {
            return String::empty;
        }

        TResult<StrataTypeMapping> retRes = MapToStrataType(analyzer, functionType->returnType);

        if (retRes.HasError())
        {
            return String::empty;
        }

        const StrataTypeMapping& retMapping = retRes.GetValue();

        // Structs/arrays need a dedicated thunk; string getters reuse fine
        // (the out-param form is accepted for property getters).
        if (retMapping.isStructValue || retMapping.isArray)
        {
            return String::empty;
        }

        if ((retMapping.isHandle || retMapping.isEnum) && !allHandleNames.Contains(retMapping.typeName))
        {
            return String::empty;
        }

        if (retMapping.typeName != propMapping.typeName)
        {
            return String::empty;
        }
    }

    return HYP_FORMAT("{}_{}", cls.name, ResolveManagedName(accessor));
}

// Picks a property's accessor symbols, reusing existing externs when possible.
bool ResolvePropertyAccessorBinding(const Analyzer& analyzer, const ClassDefinition& cls,
    const ResolvedStrataProperty& prop, const Set<String>& allHandleNames,
    const Set<String>& emittedExternNames, PropertyAccessorBinding& out)
{
    out.getterSymbol = prop.getterSymbol;
    out.getterSynthesized = true;
    out.setterSymbol = prop.setterSymbol;
    out.setterSynthesized = false;

    if (String existing = ResolveExistingAccessorSymbol(analyzer, cls, *prop.getter, prop.mapping, false, allHandleNames); existing.Any())
    {
        out.getterSymbol = existing;
        out.getterSynthesized = false;
    }
    else if (emittedExternNames.Contains(prop.getterSymbol))
    {
        return false;
    }

    if (prop.setter)
    {
        if (String existing = ResolveExistingAccessorSymbol(analyzer, cls, *prop.setter, prop.mapping, true, allHandleNames); existing.Any())
        {
            out.setterSymbol = existing;
        }
        else
        {
            out.setterSynthesized = true;

            if (emittedExternNames.Contains(prop.setterSymbol))
            {
                return false;
            }
        }
    }

    return true;
}

Set<String> CollectDeclaredTypeNames(const Analyzer& analyzer)
{
    Set<String> typeNames;

    // The base for all class types in the engine.
    typeNames.Insert("ObjectBase");

    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, ClassDefinition>& pair : mod->GetClasses())
        {
            const ClassDefinition& cls = pair.second;

            if ((cls.type == ClassDefinitionType::Class || cls.type == ClassDefinitionType::Enum)
                && IsStrataScriptable(analyzer, cls))
            {
                typeNames.Insert(cls.name);
            }
        }
    }

    return typeNames;
}

} // namespace BindingsShared
} // namespace CodeGen
} // namespace Hyperion
