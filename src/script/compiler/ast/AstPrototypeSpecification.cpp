#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>
#include <script/Hasher.hpp>

namespace hyperion::compiler {

AstPrototypeSpecification::AstPrototypeSpecification(
    const RC<AstExpression> &expr,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_expr(expr)
{
}

void AstPrototypeSpecification::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    const AstExpression *value_of = m_expr->GetDeepValueOf();
    AssertThrow(value_of != nullptr);

    SymbolTypePtr_t held_type = value_of->GetHeldType();

    if (held_type == nullptr) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_type,
            m_location,
            value_of->GetExprType()->ToString()
        ));

        return;
    }

    held_type = held_type->GetUnaliased();

    if (held_type->IsEnumType()) {
        AssertThrow(held_type->GetGenericInstanceInfo().m_generic_args.Size() == 1);

        auto enum_underlying_type = held_type->GetGenericInstanceInfo().m_generic_args.Front().m_type;
        AssertThrow(enum_underlying_type != nullptr);
        enum_underlying_type = enum_underlying_type->GetUnaliased();

        // for enum types, we use the underlying type.
        held_type = enum_underlying_type;
    }

    m_symbol_type = held_type;
    FindPrototypeType(held_type);

    // m_symbol_type = BuiltinTypes::UNDEFINED;
    // m_prototype_type = BuiltinTypes::UNDEFINED;
    
    // if (held_type->IsAnyType()) {
    //     // it is a dynamic type
    //     m_symbol_type = BuiltinTypes::ANY;
    //     m_prototype_type = BuiltinTypes::ANY;
    //     m_default_value = BuiltinTypes::ANY->GetDefaultValue();

    //     return;
    // }
    
    // if (held_type->IsPlaceholderType()) {
    //     m_symbol_type = BuiltinTypes::PLACEHOLDER;
    //     m_prototype_type = BuiltinTypes::PLACEHOLDER;
    //     m_default_value = BuiltinTypes::PLACEHOLDER->GetDefaultValue();

    //     return;
    // }

    // if (FindPrototypeType(held_type)) {
    //     m_symbol_type = held_type;
    // } else {
    //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
    //         LEVEL_ERROR,
    //         Msg_type_missing_prototype,
    //         m_location,
    //         held_type->ToString()
    //     ));
    // }

        // if (found_symbol_type != held_type && found_symbol_type != nullptr) {
        //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        //         LEVEL_ERROR,
        //         Msg_type_missing_prototype,
        //         m_location,
        //         expr_type->ToString() + " (expanded: " + found_symbol_type->ToString() + ")"
        //     ));
        // } else {
        //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        //         LEVEL_ERROR,
        //         Msg_type_missing_prototype,
        //         m_location,
        //         expr_type->ToString()
        //     ));
        // }
    // }

    AssertThrow(m_symbol_type != nullptr);
    // AssertThrow(m_prototype_type != nullptr);
}

std::unique_ptr<Buildable> AstPrototypeSpecification::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    return m_expr->Build(visitor, mod);
}

void AstPrototypeSpecification::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

bool AstPrototypeSpecification::FindPrototypeType(const SymbolTypePtr_t &symbol_type)
{
    if (symbol_type->GetTypeClass() == TYPE_BUILTIN || symbol_type->IsGenericParameter()) {
        m_prototype_type = symbol_type;
        m_prototype_type = m_prototype_type->GetUnaliased();

        m_default_value = symbol_type->GetDefaultValue();

        return true;
    }

    SymbolTypeMember proto_member;

    if (symbol_type->FindMember("$proto", proto_member)) {
        m_prototype_type = proto_member.type;
        AssertThrow(m_prototype_type != nullptr);
        m_prototype_type = m_prototype_type->GetUnaliased();

        if (m_prototype_type->GetTypeClass() == TYPE_BUILTIN) {
            m_default_value = proto_member.expr;
        }

        return true;
    }

    return false;
}

RC<AstStatement> AstPrototypeSpecification::Clone() const
{
    return CloneImpl();
}

Tribool AstPrototypeSpecification::IsTrue() const
{
    return Tribool::True();
}

bool AstPrototypeSpecification::MayHaveSideEffects() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->MayHaveSideEffects();
}

SymbolTypePtr_t AstPrototypeSpecification::GetExprType() const
{
    if (m_expr != nullptr) {
        return m_expr->GetExprType();
    }

    return nullptr;
}

const AstExpression *AstPrototypeSpecification::GetValueOf() const
{
    if (m_expr != nullptr) {
        return m_expr->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression *AstPrototypeSpecification::GetDeepValueOf() const
{
    if (m_expr != nullptr) {
        return m_expr->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

SymbolTypePtr_t AstPrototypeSpecification::GetHeldType() const
{
    if (m_symbol_type != nullptr) {
        return m_symbol_type;
    }

    return AstExpression::GetHeldType();
}

} // namespace hyperion::compiler
