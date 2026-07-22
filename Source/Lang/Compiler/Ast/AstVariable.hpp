#pragma once

#include <Lang/Compiler/Ast/AstIdentifier.hpp>
#include <Lang/Compiler/Ast/AstMember.hpp>
#include <Core/Containers/String.hpp>

namespace Hyperion {

class AstTypeRef;

HYP_CLASS()
class AstVariable final : public AstIdentifier
{
    HYP_OBJECT_BODY(AstVariable);

public:
    AstVariable(const String& name, const SourceLocation& location);
    virtual ~AstVariable() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual bool IsLiteral() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;
    virtual bool IsMutable() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;
    virtual AstExpression* GetTarget() const override;

    virtual Handle<AstStatement> Clone() const override;

private:
    // set while analyzing
    // used to get locals from outer function in a closure
    Handle<AstMember> m_closureMemberAccess;
    Handle<AstMember> m_selfMemberAccess;
    Handle<AstTypeRef> m_typeRef;
    Handle<AstExpression> m_inlineValue;
    bool m_shouldInline : 1;
    bool m_isInRefAssignment : 1;
    bool m_isInConstAssignment : 1;

    Handle<AstVariable> CloneImpl() const
    {
        return MakeHandle<AstVariable>(
            m_name,
            m_location);
    }
};

} // namespace Hyperion
