#pragma once

#include <Lang/Compiler/Ast/AstMember.hpp>
#include <Lang/Compiler/Ast/AstArgumentList.hpp>
#include <Core/Containers/String.hpp>

namespace Hyperion {

HYP_CLASS()
class AstMemberCallExpression : public AstMember
{
    HYP_OBJECT_BODY(AstMemberCallExpression);

public:
    AstMemberCallExpression(
        const String& fieldName,
        const SharedPtr<AstExpression>& target,
        const SharedPtr<AstArgumentList>& arguments,
        const SourceLocation& location);
    virtual ~AstMemberCallExpression() override = default;

    HYP_FORCE_INLINE const SharedPtr<AstArgumentList>& GetArguments() const
    {
        return m_arguments;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;
    virtual AstExpression* GetTarget() const override;

    virtual String ToString() const override;

protected:
    SharedPtr<AstArgumentList> m_arguments;

    // set while analyzing
    Array<SharedPtr<AstArgument>> m_substitutedArgs;
    const SymbolType* m_returnType;

    SharedPtr<AstMemberCallExpression> CloneImpl() const
    {
        return SharedPtr<AstMemberCallExpression>(new AstMemberCallExpression(
            m_fieldName,
            CloneAstNode(m_target),
            CloneAstNode(m_arguments),
            m_location));
    }
};

} // namespace Hyperion
