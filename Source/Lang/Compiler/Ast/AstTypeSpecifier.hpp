#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/TypeSystem/SymbolType.hpp>

#include <string>
#include <memory>

namespace Hyperion {

HYP_CLASS(Abstract)
class AstTypeSpecifier : public AstExpression
{
    HYP_OBJECT_BODY(AstTypeSpecifier);

public:
    AstTypeSpecifier(
        const RC<AstExpression>& expr,
        const SourceLocation& location);
    virtual ~AstTypeSpecifier() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual const RC<AstExpression>& GetExpr() const
    {
        return m_expr;
    }

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;
    virtual const SymbolType* GetHeldType() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTypeSpecifier>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        return hc;
    }

protected:
    RC<AstExpression> m_expr;

    /** Set while analyzing */
    const SymbolType* m_symbolType;

private:
    RC<AstTypeSpecifier> CloneImpl() const
    {
        return RC<AstTypeSpecifier>(new AstTypeSpecifier(
            CloneAstNode(m_expr),
            m_location));
    }
};

} // namespace Hyperion
