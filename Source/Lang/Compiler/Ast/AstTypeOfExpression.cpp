#include <Lang/Compiler/Ast/AstTypeOfExpression.hpp>
#include <Lang/Compiler/Ast/AstVariable.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Core/Debug/Debug.hpp>

namespace Hyperion {

AstTypeOfExpression::AstTypeOfExpression(
    const RC<AstExpression>& expr,
    const SourceLocation& location)
    : AstTypeSpecifier(expr, location),
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
      m_typeRef(nullptr),
      m_heldType(nullptr)
#else
      m_stringExpr(nullptr)
#endif
{
}

void AstTypeOfExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    m_heldType = BuiltinTypes::s_errorType;

#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    const SymbolType* exprType = m_expr->GetExprType();

    if (exprType != nullptr)
    {
        m_heldType = exprType->GetUnaliased();
        Assert(m_heldType != nullptr);
    }

    m_typeRef.Reset(new AstTypeRef(m_heldType, m_location));
    m_typeRef->Visit(visitor, mod);
#else
    m_symbolType = BuiltinTypes::s_stringType;

    const SymbolType* exprType = nullptr;
    const SymbolType* unaliased = nullptr;

    if ((exprType = m_expr->GetExprType()) && (unaliased = exprType->GetUnaliased()))
    {
        m_stringExpr.Reset(new AstString(
            unaliased->ToString(false),
            m_location));
    }
    else
    {
        m_stringExpr.Reset(new AstString(
            BuiltinTypes::s_errorType->ToString(),
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

const SymbolType* AstTypeOfExpression::GetExprType() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetExprType();
#else
    return BuiltinTypes::s_stringType;
#endif
}

const SymbolType* AstTypeOfExpression::GetHeldType() const
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

} // namespace Hyperion
