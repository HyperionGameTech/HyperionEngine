#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/Tribool.hpp>

#include <string>

namespace hyperion::compiler {

class AstIsExpression : public AstExpression
{
public:
    AstIsExpression(
        const RC<AstExpression>& target,
        const RC<AstTypeSpecifier>& typeSpecification,
        const SourceLocation& location);

    virtual ~AstIsExpression() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypeRef GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstIsExpression>());
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());
        hc.Add(m_typeSpecification ? m_typeSpecification->GetHashCode() : HashCode());

        return hc;
    }

protected:
    RC<AstExpression> m_target;
    RC<AstTypeSpecifier> m_typeSpecification;

    // set while analyzing
    RC<AstExpression> m_overrideExpr;

    Tribool m_isType;

private:
    RC<AstIsExpression> CloneImpl() const
    {
        return RC<AstIsExpression>(new AstIsExpression(
            CloneAstNode(m_target),
            CloneAstNode(m_typeSpecification),
            m_location));
    }
};

} // namespace hyperion::compiler
