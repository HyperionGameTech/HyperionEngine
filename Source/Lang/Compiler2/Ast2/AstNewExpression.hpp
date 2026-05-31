#pragma once

#include <Lang/compiler/ast/AstExpression.hpp>
#include <Lang/compiler/ast/AstBlock.hpp>
#include <Lang/compiler/ast/AstArgumentList.hpp>
#include <Lang/compiler/ast/AstMemberCallExpression.hpp>
#include <Lang/compiler/ast/AstTypeSpecifier.hpp>
#include <Lang/compiler/type-system/SymbolType.hpp>

#include <string>

namespace Hyperion {

class AstNewExpression : public AstExpression
{
public:
    AstNewExpression(
        const RC<AstTypeSpecifier>& typeSpec,
        const RC<AstArgumentList>& argList,
        bool enableConstructorCall,
        const SourceLocation& location);
    virtual ~AstNewExpression() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;
    virtual AstExpression* GetTarget() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstNewExpression>());
        hc.Add(m_typeSpec ? m_typeSpec->GetHashCode() : HashCode());
        hc.Add(m_argList ? m_argList->GetHashCode() : HashCode());
        hc.Add(m_enableConstructorCall);

        return hc;
    }

private:
    RC<AstTypeSpecifier> m_typeSpec;
    RC<AstArgumentList> m_argList;
    bool m_enableConstructorCall : 1;

    /** Set while analyzing */
    const SymbolType* m_instanceType;
    RC<AstBlock> m_constructorBlock; // create a block to store temporary vars
    RC<AstExpression> m_constructorCall;

    RC<AstNewExpression> CloneImpl() const
    {
        return RC<AstNewExpression>(new AstNewExpression(
            CloneAstNode(m_typeSpec),
            CloneAstNode(m_argList),
            m_enableConstructorCall,
            m_location));
    }
};

} // namespace Hyperion
