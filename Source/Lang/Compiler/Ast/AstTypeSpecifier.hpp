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
        const SharedPtr<AstExpression>& expr,
        const SourceLocation& location);
    virtual ~AstTypeSpecifier() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual const SharedPtr<AstExpression>& GetExpr() const
    {
        return m_expr;
    }

    virtual SharedPtr<AstStatement> Clone() const override;

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
    SharedPtr<AstExpression> m_expr;

    /** Set while analyzing */
    const SymbolType* m_symbolType;

private:
    SharedPtr<AstTypeSpecifier> CloneImpl() const
    {
        return SharedPtr<AstTypeSpecifier>(new AstTypeSpecifier(
            CloneAstNode(m_expr),
            m_location));
    }
};

} // namespace Hyperion
