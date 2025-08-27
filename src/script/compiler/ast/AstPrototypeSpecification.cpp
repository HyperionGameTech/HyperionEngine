#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
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

    Assert(m_symbolType != nullptr);
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
