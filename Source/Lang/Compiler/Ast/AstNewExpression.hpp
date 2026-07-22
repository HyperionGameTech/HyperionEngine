#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>
#include <Lang/Compiler/Ast/AstArgumentList.hpp>
#include <Lang/Compiler/Ast/AstMemberCallExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/TypeSystem/SymbolType.hpp>

#include <string>

namespace Hyperion {

HYP_CLASS()
class AstNewExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstNewExpression);

public:
    AstNewExpression(
        const Handle<AstTypeSpecifier>& typeSpec,
        const Handle<AstArgumentList>& argList,
        bool enableConstructorCall,
        const SourceLocation& location);
    virtual ~AstNewExpression() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

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
    Handle<AstTypeSpecifier> m_typeSpec;
    Handle<AstArgumentList> m_argList;
    bool m_enableConstructorCall : 1;

    /** Set while analyzing */
    const SymbolType* m_instanceType;
    Handle<AstBlock> m_constructorBlock; // create a block to store temporary vars
    Handle<AstExpression> m_constructorCall;

    Handle<AstNewExpression> CloneImpl() const
    {
        return MakeHandle<AstNewExpression>(
            CloneAstNode(m_typeSpec),
            CloneAstNode(m_argList),
            m_enableConstructorCall,
            m_location);
    }
};

} // namespace Hyperion
