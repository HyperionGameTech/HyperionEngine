#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Tribool.hpp>

#include <string>

namespace Hyperion {

HYP_CLASS()
class AstIsExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstIsExpression);

public:
    AstIsExpression(
        const Handle<AstExpression>& target,
        const Handle<AstTypeSpecifier>& typeSpec,
        const SourceLocation& location);

    virtual ~AstIsExpression() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstIsExpression>());
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());
        hc.Add(m_typeSpec ? m_typeSpec->GetHashCode() : HashCode());

        return hc;
    }

protected:
    Handle<AstExpression> m_target;
    Handle<AstTypeSpecifier> m_typeSpec;

    // set while analyzing
    Tribool m_isType;

private:
    Handle<AstIsExpression> CloneImpl() const
    {
        return MakeHandle<AstIsExpression>(
            CloneAstNode(m_target),
            CloneAstNode(m_typeSpec),
            m_location);
    }
};

} // namespace Hyperion
