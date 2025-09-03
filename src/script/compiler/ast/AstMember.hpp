#pragma once

#include <script/compiler/ast/AstIdentifier.hpp>
#include <core/containers/String.hpp>

namespace hyperion::compiler {

class AstTypeSpecifier;

class AstMember : public AstExpression
{
public:
    AstMember(
        const String& fieldName,
        const RC<AstExpression>& target,
        const SourceLocation& location);
    virtual ~AstMember() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual SymbolTypeRef GetExprType() const override;
    virtual SymbolTypeRef GetHeldType() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;
    virtual AstExpression* GetTarget() const override;
    virtual bool IsMutable() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstMember>());
        hc.Add(m_fieldName);
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());

        return hc;
    }

protected:
    String m_fieldName;
    RC<AstExpression> m_target;

    // set while analyzing
    SymbolTypeRef m_symbolType;
    SymbolTypeRef m_targetType;
    SymbolTypeRef m_heldType;
    RC<AstTypeSpecifier> m_typeSpec;
    RC<AstExpression> m_overrideExpr;
    uint32 m_foundIndex;
    bool m_enableGenericMemberSubstitution;

    RC<AstMember> CloneImpl() const
    {
        return RC<AstMember>(new AstMember(
            m_fieldName,
            CloneAstNode(m_target),
            m_location));
    }
};

} // namespace hyperion::compiler
