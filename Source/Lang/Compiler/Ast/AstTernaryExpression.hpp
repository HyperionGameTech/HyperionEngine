#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

HYP_CLASS()
class AstTernaryExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstTernaryExpression);

public:
    AstTernaryExpression(
        const Handle<AstExpression>& conditional,
        const Handle<AstExpression>& left,
        const Handle<AstExpression>& right,
        const SourceLocation& location);
    virtual ~AstTernaryExpression() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

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
    Handle<AstExpression> m_conditional;
    Handle<AstExpression> m_left;
    Handle<AstExpression> m_right;

    Handle<AstTernaryExpression> CloneImpl() const
    {
        return MakeHandle<AstTernaryExpression>(
                CloneAstNode(m_conditional),
                CloneAstNode(m_left),
                CloneAstNode(m_right),
                m_location);
    }
};

} // namespace Hyperion
