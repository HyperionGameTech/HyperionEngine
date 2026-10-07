/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_BINDINGS_SHARED_HPP
#define HYPERION_CODEGEN_BINDINGS_SHARED_HPP

#include <parser/Parser.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Set.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {
namespace CodeGen {

class Analyzer;
struct ClassDefinition;
struct MemberDef;

// Which reflected members are bindable and how they are named and typed. Shared by the C thunk generator and the
// Strata declaration generator, which must agree on every symbol.
namespace BindingsShared {

// A property resolved to its Strata representation, with accessor symbols
// (synthesized symbols get dedicated thunks in CBindingGenerator::EmitThunks).
struct ResolvedStrataProperty
{
    String name;
    StrataTypeMapping mapping;
    const MemberDef* getter = nullptr;
    const MemberDef* setter = nullptr;
    const ASTType* getterValueType = nullptr;
    const ASTType* setterValueType = nullptr;
    String getterSymbol;
    String setterSymbol;
    String condition;
};

struct StrataAccessorBinding
{
    const MemberDef* member = nullptr;
    const ASTType* valueType = nullptr;
    StrataTypeMapping mapping;
};

struct PropertyAccessorBinding
{
    String getterSymbol;
    bool getterSynthesized = false;
    String setterSymbol;
    bool setterSynthesized = false;
};

bool IsStrataScriptable(const Analyzer& analyzer, const ClassDefinition& cls);
bool MemberIsStrataScriptable(const MemberDef& member);

String ResolveManagedName(const MemberDef& member);
String StructParamModifier(const ASTType* paramType);
String SelfParamDecl(const ClassDefinition& cls, bool isConstMethod);

TResult<StrataAccessorBinding> ResolveStrataAccessorBinding(const Analyzer& analyzer, const String& propertyName,
    const MemberDef& refMember, bool isSetter, const Set<String>& allHandleNames);

TResult<ResolvedStrataProperty> BuildResolvedStrataProperty(const Analyzer& analyzer, const ClassDefinition& cls,
    const String& name, const MemberDef* getter, const MemberDef* setter, const String& condition,
    const Set<String>& allHandleNames);

TResult<ResolvedStrataProperty> ResolveStrataProperty(const Analyzer& analyzer, const ClassDefinition& cls,
    const MemberDef& member, const Set<String>& allHandleNames);

Array<ResolvedStrataProperty> CollectImplProperties(const Analyzer& analyzer, const ClassDefinition& cls,
    const Set<String>& allHandleNames);

String PropertyAccessorCondition(const ResolvedStrataProperty& prop, const MemberDef* accessor);

bool IsEmittedOverload(const ClassDefinition& cls, const MemberDef& member);
bool IsPropertySetterIdiom(const ClassDefinition& cls, const MemberDef& member);

String ResolveExistingAccessorSymbol(const Analyzer& analyzer, const ClassDefinition& cls,
    const MemberDef& accessor, const StrataTypeMapping& propMapping, bool isSetter,
    const Set<String>& allHandleNames);

bool ResolvePropertyAccessorBinding(const Analyzer& analyzer, const ClassDefinition& cls,
    const ResolvedStrataProperty& prop, const Set<String>& allHandleNames,
    const Set<String>& emittedExternNames, PropertyAccessorBinding& out);

String StrataEnumUnderlyingType(const String& cxxUnderlying);
String TranslateLimitMacros(const String& value);

// A Handle<T> returned by value hands its reference to the caller, who must release it.
bool IsRetainedHandleReturn(const StrataTypeMapping& returnMapping, const ASTType* returnType);

// Whether the C generator binds this method. Strata declares a subset: it has no way to release an owned handle and
// takes at most two parameters. It must still reserve the symbol of a method it skips, so that both generators pick
// the same overload for every name.
bool ResolveMethodBindability(const Analyzer& analyzer, const ClassDefinition& cls, const MemberDef& member,
    const Set<String>& declaredTypeNames, bool& outStrataBindable);

// A scriptable enum as other languages see it: an integer type and named constants.
struct BindingEnum
{
    String name;
    String underlying;                     // binding scalar name ("byte", "uint", ...)
    Array<Pair<String, String>> constants; // name -> value, only where the value is a plain literal or `1 << n`
};

Array<BindingEnum> CollectBindingEnums(const Analyzer& analyzer);

// Every scriptable class/struct name plus every scriptable enum name: the types a binding may refer to.
Set<String> CollectDeclaredTypeNames(const Analyzer& analyzer);

} // namespace BindingsShared
} // namespace CodeGen
} // namespace Hyperion

#endif
