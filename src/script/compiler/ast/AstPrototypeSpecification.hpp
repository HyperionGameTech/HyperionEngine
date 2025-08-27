#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

class AstPrototypeSpecification : public AstExpression
{
public:
    AstPrototypeSpecification(
        const RC<AstExpression>& expr,
        const SourceLocation& location);
    virtual ~AstPrototypeSpecification() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    const RC<AstExpression>& GetDefaultValue() const
    {
        return m_defaultValue;
    }
    virtual const RC<AstExpression>& GetExpr() const
    {
        return m_expr;
    }

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypeRef GetExprType() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

    virtual SymbolTypeRef GetHeldType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstPrototypeSpecification>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        return hc;
    }

protected:
    RC<AstExpression> m_expr;

    /** Set while analyzing */
    SymbolTypeRef m_symbolType;
    RC<AstExpression> m_defaultValue;

private:
    RC<AstPrototypeSpecification> CloneImpl() const
    {
        return RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
            CloneAstNode(m_expr),
            m_location));
    }
};

} // namespace hyperion::compiler
