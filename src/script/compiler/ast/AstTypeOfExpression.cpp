#include <script/compiler/ast/AstTypeOfExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

AstTypeOfExpression::AstTypeOfExpression(
    const RC<AstExpression>& expr,
    const SourceLocation& location)
    : AstTypeSpecifier(expr, location)
{
}

void AstTypeOfExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    m_heldType = BuiltinTypes::UNDEFINED;

    auto* valueOf = m_expr->GetDeepValueOf();
    Assert(valueOf != nullptr);

#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    if (SymbolTypeRef exprType = valueOf->GetExprType())
    {
        m_heldType = exprType->GetUnaliased();
    }

    Assert(m_heldType != nullptr);

    m_typeRef.Reset(new AstTypeRef(
        m_heldType,
        m_location));

    m_typeRef->Visit(visitor, mod);
#else
    m_symbolType = BuiltinTypes::STRING;

    SymbolTypeRef exprType;
    SymbolTypeRef unaliased;

    if ((exprType = m_expr->GetExprType()) && (unaliased = exprType->GetUnaliased()))
    {
        m_stringExpr.Reset(new AstString(
            unaliased->ToString(false),
            m_location));
    }
    else
    {
        m_stringExpr.Reset(new AstString(
            BuiltinTypes::UNDEFINED->ToString(),
            m_location));
    }

    m_stringExpr->Visit(visitor, mod);
#endif
}

UniquePtr<Buildable> AstTypeOfExpression::Build(AstVisitor* visitor, Module* mod)
{
    auto chunk = BytecodeUtil::Make<BytecodeChunk>();
    chunk->Append(AstTypeSpecifier::Build(visitor, mod));

#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    Assert(m_typeRef != nullptr);

    chunk->Append(m_typeRef->Build(visitor, mod));
#else
    Assert(m_stringExpr != nullptr);
    chunk->Append(m_stringExpr->Build(visitor, mod));
#endif

    return chunk;
}

void AstTypeOfExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    AstTypeSpecifier::Optimize(visitor, mod);

#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    Assert(m_typeRef != nullptr);

    return m_typeRef->Optimize(visitor, mod);
#else
    Assert(m_stringExpr != nullptr);
    m_stringExpr->Optimize(visitor, mod);
#endif
}

RC<AstStatement> AstTypeOfExpression::Clone() const
{
    return CloneImpl();
}

SymbolTypeRef AstTypeOfExpression::GetExprType() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetExprType();
#else
    return BuiltinTypes::STRING;
#endif
}

SymbolTypeRef AstTypeOfExpression::GetHeldType() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetHeldType();
#else
    return AstExpression::GetHeldType();
#endif
}

const AstExpression* AstTypeOfExpression::GetValueOf() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetValueOf();
#else
    Assert(m_stringExpr != nullptr);

    return m_stringExpr->GetValueOf();
#endif
}

const AstExpression* AstTypeOfExpression::GetDeepValueOf() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetDeepValueOf();
#else
    Assert(m_stringExpr != nullptr);

    return m_stringExpr->GetDeepValueOf();
#endif
}

} // namespace hyperion
