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

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

AstPrototypeSpecification::AstPrototypeSpecification(
    const RC<AstExpression>& expr,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr)
{
}

void AstPrototypeSpecification::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    Assert(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    const AstExpression* valueOf = m_expr->GetDeepValueOf();
    Assert(valueOf != nullptr);

    SymbolTypeRef heldType = valueOf->GetHeldType();

    if (heldType == nullptr)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_type,
            m_location,
            valueOf->GetExprType()->ToString()));

        return;
    }

    heldType = heldType->GetUnaliased();

    if (heldType->IsEnumType())
    {
        Assert(heldType->GetGenericInstanceInfo().m_genericArgs.Size() == 1);

        auto enumUnderlyingType = heldType->GetGenericInstanceInfo().m_genericArgs.Front().m_type;
        Assert(enumUnderlyingType != nullptr);
        enumUnderlyingType = enumUnderlyingType->GetUnaliased();

        // for enum types, we use the underlying type.
        heldType = enumUnderlyingType;
    }

    m_symbolType = heldType;
    FindPrototypeType(heldType);

    // m_symbolType = BuiltinTypes::UNDEFINED;
    // m_prototypeType = BuiltinTypes::UNDEFINED;

    // if (heldType->IsAnyType()) {
    //     // it is a dynamic type
    //     m_symbolType = BuiltinTypes::ANY;
    //     m_prototypeType = BuiltinTypes::ANY;
    //     m_defaultValue = BuiltinTypes::ANY->GetDefaultValue();

    //     return;
    // }

    // if (heldType->IsPlaceholderType()) {
    //     m_symbolType = BuiltinTypes::PLACEHOLDER;
    //     m_prototypeType = BuiltinTypes::PLACEHOLDER;
    //     m_defaultValue = BuiltinTypes::PLACEHOLDER->GetDefaultValue();

    //     return;
    // }

    // if (FindPrototypeType(heldType)) {
    //     m_symbolType = heldType;
    // } else {
    //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
    //         LEVEL_ERROR,
    //         Msg_type_missing_prototype,
    //         m_location,
    //         heldType->ToString()
    //     ));
    // }

    // if (foundSymbolType != heldType && foundSymbolType != nullptr) {
    //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
    //         LEVEL_ERROR,
    //         Msg_type_missing_prototype,
    //         m_location,
    //         exprType->ToString() + " (expanded: " + foundSymbolType->ToString() + ")"
    //     ));
    // } else {
    //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
    //         LEVEL_ERROR,
    //         Msg_type_missing_prototype,
    //         m_location,
    //         exprType->ToString()
    //     ));
    // }
    // }

    Assert(m_symbolType != nullptr);
    // Assert(m_prototypeType != nullptr);
}

UniquePtr<Buildable> AstPrototypeSpecification::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    return m_expr->Build(visitor, mod);
}

void AstPrototypeSpecification::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

bool AstPrototypeSpecification::FindPrototypeType(const SymbolTypeRef& symbolType)
{
    if (symbolType->GetTypeClass() == TYPE_BUILTIN || symbolType->IsGenericParameter())
    {
        m_prototypeType = symbolType;
        m_prototypeType = m_prototypeType->GetUnaliased();

        m_defaultValue = symbolType->GetDefaultValue();

        return true;
    }

    SymbolTypeMember protoMember;

    if (symbolType->FindMember("$proto", protoMember))
    {
        m_prototypeType = protoMember.type;
        Assert(m_prototypeType != nullptr);
        m_prototypeType = m_prototypeType->GetUnaliased();

        if (m_prototypeType->GetTypeClass() == TYPE_BUILTIN)
        {
            m_defaultValue = protoMember.expr;
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
    Assert(m_expr != nullptr);
    return m_expr->MayHaveSideEffects();
}

SymbolTypeRef AstPrototypeSpecification::GetExprType() const
{
    if (m_expr != nullptr)
    {
        return m_expr->GetExprType();
    }

    return nullptr;
}

const AstExpression* AstPrototypeSpecification::GetValueOf() const
{
    if (m_expr != nullptr)
    {
        return m_expr->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression* AstPrototypeSpecification::GetDeepValueOf() const
{
    if (m_expr != nullptr)
    {
        return m_expr->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

SymbolTypeRef AstPrototypeSpecification::GetHeldType() const
{
    if (m_symbolType != nullptr)
    {
        return m_symbolType;
    }

    return AstExpression::GetHeldType();
}

} // namespace hyperion::compiler
