#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>

namespace hyperion::compiler {

class AstNewExpression : public AstExpression
{
public:
    AstNewExpression(
        const RC<AstPrototypeSpecification>& proto,
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
    virtual SymbolTypeRef GetExprType() const override;
    virtual AstExpression* GetTarget() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstNewExpression>());
        hc.Add(m_proto ? m_proto->GetHashCode() : HashCode());
        hc.Add(m_argList ? m_argList->GetHashCode() : HashCode());
        hc.Add(m_enableConstructorCall);

        return hc;
    }

private:
    RC<AstPrototypeSpecification> m_proto;
    RC<AstArgumentList> m_argList;
    bool m_enableConstructorCall;

    /** Set while analyzing */
    RC<AstExpression> m_objectValue;
    SymbolTypeRef m_instanceType;
    RC<AstBlock> m_constructorBlock; // create a block to store temporary vars
    RC<AstExpression> m_constructorCall;

    RC<AstNewExpression> CloneImpl() const
    {
        return RC<AstNewExpression>(new AstNewExpression(
            CloneAstNode(m_proto),
            CloneAstNode(m_argList),
            m_enableConstructorCall,
            m_location));
    }
};

} // namespace hyperion::compiler
