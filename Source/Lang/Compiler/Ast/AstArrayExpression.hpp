#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>

namespace Hyperion {

class AstTypeSpecifier;

HYP_CLASS()
class AstArrayExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstArrayExpression);

public:
    AstArrayExpression(
        const Array<Handle<AstExpression>>& members,
        const SourceLocation& location);
    virtual ~AstArrayExpression() = default;

    HYP_FORCE_INLINE Array<Handle<AstExpression>>& GetMembers()
    {
        return m_members;
    }

    HYP_FORCE_INLINE const Array<Handle<AstExpression>>& GetMembers() const
    {
        return m_members;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;
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
    Array<Handle<AstExpression>> m_members;

    // set while analyzing
    Array<Handle<AstExpression>> m_replacedMembers;
    const SymbolType* m_heldType;
    const SymbolType* m_exprType;
    Handle<AstExpression> m_arrayFromCall;

    Handle<AstArrayExpression> CloneImpl() const
    {
        return MakeHandle<AstArrayExpression>(
            CloneAllAstNodes(m_members),
            m_location);
    }
};

} // namespace Hyperion
