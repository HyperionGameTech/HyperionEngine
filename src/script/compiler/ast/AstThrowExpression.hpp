#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

class AstThrowExpression : public AstExpression
{
public:
    AstThrowExpression(
        const RC<AstExpression>& expr,
        const SourceLocation& location);
    virtual ~AstThrowExpression() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstThrowExpression>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        return hc;
    }

protected:
    RC<AstExpression> m_expr;

private:
    RC<AstThrowExpression> CloneImpl() const
    {
        return RC<AstThrowExpression>(new AstThrowExpression(
            CloneAstNode(m_expr),
            m_location));
    }
};

} // namespace Hyperion
