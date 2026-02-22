#pragma once

#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <Core/containers/String.hpp>

namespace Hyperion {

class AstMemberCallExpression : public AstMember
{
public:
    AstMemberCallExpression(
        const String& fieldName,
        const RC<AstExpression>& target,
        const RC<AstArgumentList>& arguments,
        const SourceLocation& location);
    virtual ~AstMemberCallExpression() override = default;

    HYP_FORCE_INLINE const RC<AstArgumentList>& GetArguments() const
    {
        return m_arguments;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;
    virtual AstExpression* GetTarget() const override;

    virtual String ToString() const override;

protected:
    RC<AstArgumentList> m_arguments;

    // set while analyzing
    Array<RC<AstArgument>> m_substitutedArgs;
    const SymbolType* m_returnType;

    RC<AstMemberCallExpression> CloneImpl() const
    {
        return RC<AstMemberCallExpression>(new AstMemberCallExpression(
            m_fieldName,
            CloneAstNode(m_target),
            CloneAstNode(m_arguments),
            m_location));
    }
};

} // namespace Hyperion
