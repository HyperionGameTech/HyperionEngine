#pragma once

#include <script/compiler/ast/AstExpression.hpp>

namespace hyperion {

class AstTypeSpecifier;

class AstArrayExpression : public AstExpression
{
public:
    AstArrayExpression(
        const Array<RC<AstExpression>>& members,
        const SourceLocation& location);
    virtual ~AstArrayExpression() = default;

    const Array<RC<AstExpression>>& GetMembers() const
    {
        return m_members;
    };

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypeRef GetExprType() const override;
    virtual bool IsMutable() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstArrayExpression>());

        for (auto& member : m_members)
        {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        return hc;
    }

protected:
    Array<RC<AstExpression>> m_members;

    // set while analyzing
    Array<RC<AstExpression>> m_replacedMembers;
    SymbolTypeRef m_heldType;
    SymbolTypeRef m_exprType;
    RC<AstExpression> m_arrayFromCall;

    RC<AstArrayExpression> CloneImpl() const
    {
        return RC<AstArrayExpression>(new AstArrayExpression(
            CloneAllAstNodes(m_members),
            m_location));
    }
};

} // namespace hyperion
