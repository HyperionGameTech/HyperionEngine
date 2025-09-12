#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Operator.hpp>

namespace hyperion {

class AstBinaryExpression;
class AstBlock;

class AstUnaryExpression : public AstExpression
{
public:
    AstUnaryExpression(
        const RC<AstExpression>& expr,
        const Operator* op,
        bool isPostfixVersion,
        const SourceLocation& location);

    HYP_FORCE_INLINE const RC<AstExpression>& GetExpr() const
    {
        return m_expr;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual ConstantValue GetConstantValue() const override;

    virtual SymbolTypeRef GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstUnaryExpression>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());
        hc.Add(m_op ? m_op->GetHashCode() : HashCode());
        hc.Add(m_isPostfixVersion);

        return hc;
    }

private:
    RC<AstExpression> m_expr;
    const Operator* m_op;
    bool m_isPostfixVersion;

    // set while analyzing
    bool m_folded;

    RC<AstBinaryExpression> m_binExpr; // for operators that modify their argument
    RC<AstBlock> m_overrideBlock;      // for postfix ++/--

    RC<AstUnaryExpression> CloneImpl() const
    {
        return RC<AstUnaryExpression>(new AstUnaryExpression(
            CloneAstNode(m_expr),
            m_op,
            m_isPostfixVersion,
            m_location));
    }
};

} // namespace hyperion
