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
        const SharedPtr<AstExpression>& expr,
        const Operator* op,
        bool isPostfixVersion,
        const SourceLocation& location);

    HYP_FORCE_INLINE const SharedPtr<AstExpression>& GetExpr() const
    {
        return m_expr;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

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
    SharedPtr<AstExpression> m_expr;
    const Operator* m_op;
    bool m_isPostfixVersion : 1;

    // set while analyzing
    bool m_folded : 1;

    SharedPtr<AstBinaryExpression> m_binExpr; // for operators that modify their argument
    SharedPtr<AstBlock> m_overrideBlock;      // for postfix ++/--

    SharedPtr<AstUnaryExpression> CloneImpl() const
    {
        return SharedPtr<AstUnaryExpression>(new AstUnaryExpression(
            CloneAstNode(m_expr),
            m_op,
            m_isPostfixVersion,
            m_location));
    }
};

} // namespace Hyperion
