#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

class AstTernaryExpression : public AstExpression
{
public:
    AstTernaryExpression(
        const RC<AstExpression>& conditional,
        const RC<AstExpression>& left,
        const RC<AstExpression>& right,
        const SourceLocation& location);
    virtual ~AstTernaryExpression() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;

    virtual bool IsLiteral() const override;
    virtual bool IsMutable() const override;
    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTernaryExpression>());
        hc.Add(m_conditional ? m_conditional->GetHashCode() : HashCode());
        hc.Add(m_left ? m_left->GetHashCode() : HashCode());
        hc.Add(m_right ? m_right->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstExpression> m_conditional;
    RC<AstExpression> m_left;
    RC<AstExpression> m_right;

    RC<AstTernaryExpression> CloneImpl() const
    {
        return RC<AstTernaryExpression>(
            new AstTernaryExpression(
                CloneAstNode(m_conditional),
                CloneAstNode(m_left),
                CloneAstNode(m_right),
                m_location));
    }
};

} // namespace Hyperion
