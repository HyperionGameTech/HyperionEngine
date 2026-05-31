#pragma once

#include <Lang/compiler/ast/AstIdentifier.hpp>
#include <Core/containers/String.hpp>

namespace Hyperion {

class AstTypeRef;

class AstMember : public AstExpression
{
public:
    AstMember(
        const String& fieldName,
        const RC<AstExpression>& target,
        const SourceLocation& location);
    virtual ~AstMember() = default;

    HYP_FORCE_INLINE bool IsStaticField() const
    {
        return m_isStaticField;
    }

    HYP_FORCE_INLINE bool IsStaticMethod() const
    {
        return m_isStaticMethod;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual const SymbolType* GetExprType() const override;
    virtual const SymbolType* GetHeldType() const override;
    virtual const SymbolType* GetTargetType() const override;

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
    const SymbolType* m_symbolType;
    const SymbolType* m_targetType;
    const SymbolType* m_heldType;

    RC<AstTypeRef> m_typeRef;
    uint32 m_foundIndex;
    bool m_isStaticField : 1;
    bool m_isStaticMethod : 1;
    bool m_isConst : 1;

    RC<AstMember> CloneImpl() const
    {
        return RC<AstMember>(new AstMember(
            m_fieldName,
            CloneAstNode(m_target),
            m_location));
    }
};

} // namespace Hyperion
