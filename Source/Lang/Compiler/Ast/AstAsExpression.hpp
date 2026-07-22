#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Tribool.hpp>

namespace Hyperion {

class AstTypeRef;

HYP_CLASS()
class AstAsExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstAsExpression);

public:
    AstAsExpression(
        const Handle<AstExpression>& target,
        const Handle<AstTypeSpecifier>& typeSpecification,
        const SourceLocation& location);

    virtual ~AstAsExpression() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual bool IsLiteral() const override;
    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual ConstantValue GetConstantValue() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual Handle<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstAsExpression>());
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());
        hc.Add(m_typeSpecification ? m_typeSpecification->GetHashCode() : HashCode());

        return hc;
    }

protected:
    Handle<AstExpression> m_target;
    Handle<AstTypeSpecifier> m_typeSpecification;

    // set while analyzing
    Handle<AstTypeRef> m_typeRef;
    const SymbolType* m_resultType;
    Tribool m_isType;

private:
    Handle<AstAsExpression> CloneImpl() const
    {
        return MakeHandle<AstAsExpression>(
            CloneAstNode(m_target),
            CloneAstNode(m_typeSpecification),
            m_location);
    }
};

} // namespace Hyperion
