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

namespace BindingsShared {

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

Set<String> CollectDeclaredTypeNames(const Analyzer& analyzer);

} // namespace BindingsShared
} // namespace CodeGen
} // namespace Hyperion

#endif
