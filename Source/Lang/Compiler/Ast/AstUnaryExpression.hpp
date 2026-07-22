#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Operator.hpp>

namespace Hyperion {

class AstBinaryExpression;
class AstBlock;

HYP_CLASS()
class AstUnaryExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstUnaryExpression);

public:
    AstUnaryExpression(
        const Handle<AstExpression>& expr,
        const Operator* op,
        bool isPostfixVersion,
        const SourceLocation& location);

    HYP_FORCE_INLINE const Handle<AstExpression>& GetExpr() const
    {
        return m_expr;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual ConstantValue GetConstantValue() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstUnaryExpression>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());
        hc.Add(m_op ? m_op->GetHashCode() : HashCode());
        hc.Add(m_isPostfixVersion);

        return hc;
    }

private:
    Handle<AstExpression> m_expr;
    const Operator* m_op;
    bool m_isPostfixVersion : 1;

    // set while analyzing
    bool m_folded : 1;

    Handle<AstBinaryExpression> m_binExpr; // for operators that modify their argument
    Handle<AstBlock> m_overrideBlock;      // for postfix ++/--

    Handle<AstUnaryExpression> CloneImpl() const
    {
        return MakeHandle<AstUnaryExpression>(
            CloneAstNode(m_expr),
            m_op,
            m_isPostfixVersion,
            m_location);
    }
};

} // namespace Hyperion
