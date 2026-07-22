#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

HYP_CLASS()
class AstThrowExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstThrowExpression);

public:
    AstThrowExpression(
        const Handle<AstExpression>& expr,
        const SourceLocation& location);
    virtual ~AstThrowExpression() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

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
    Handle<AstExpression> m_expr;

private:
    Handle<AstThrowExpression> CloneImpl() const
    {
        return MakeHandle<AstThrowExpression>(
            CloneAstNode(m_expr),
            m_location);
    }
};

} // namespace Hyperion
