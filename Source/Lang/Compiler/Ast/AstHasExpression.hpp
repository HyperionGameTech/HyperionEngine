#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Core/Containers/String.hpp>

#include <string>

namespace Hyperion {

HYP_CLASS()
class AstHasExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstHasExpression);

public:
    AstHasExpression(
        const Handle<AstStatement>& target,
        const String& fieldName,
        const SourceLocation& location);
    virtual ~AstHasExpression() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstHasExpression>());
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());
        hc.Add(m_fieldName);

        return hc;
    }

protected:
    Handle<AstStatement> m_target;
    String m_fieldName;

    // set while analyzing
    Tribool m_hasMember;
    // is it a check if an expression has the member,
    // or is it a check if a type has a member?
    bool m_isExpr : 1;
    bool m_hasSideEffects : 1;

private:
    Handle<AstHasExpression> CloneImpl() const
    {
        return MakeHandle<AstHasExpression>(
            CloneAstNode(m_target),
            m_fieldName,
            m_location);
    }
};

} // namespace Hyperion
