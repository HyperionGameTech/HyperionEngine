#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Enums.hpp>

#include <string>
#include <vector>
#include <memory>

namespace Hyperion {

class AstVariableDeclaration;

HYP_CLASS()
class AstArrayAccess : public AstExpression
{
    HYP_OBJECT_BODY(AstArrayAccess);

public:
    AstArrayAccess(
        const Handle<AstExpression>& target,
        const Handle<AstExpression>& index,
        bool operatorOverloadingEnabled,
        const SourceLocation& location);
    virtual ~AstArrayAccess() = default;

    HYP_FORCE_INLINE const Handle<AstExpression>& GetIndex() const
    {
        return m_index;
    }

    HYP_FORCE_INLINE bool IsOperatorOverloadingEnabled() const
    {
        return m_operatorOverloadingEnabled;
    }

    HYP_FORCE_INLINE void SetIsOperatorOverloadingEnabled(bool operatorOverloadingEnabled)
    {
        m_operatorOverloadingEnabled = operatorOverloadingEnabled;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;
    virtual AstExpression* GetTarget() const override;
    virtual bool IsMutable() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstArrayAccess>());
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());
        hc.Add(m_index ? m_index->GetHashCode() : HashCode());
        hc.Add(m_operatorOverloadingEnabled);

        return hc;
    }

private:
    Handle<AstExpression> m_target;
    Handle<AstExpression> m_index;
    bool m_operatorOverloadingEnabled;

    // set while analyzing
    Handle<AstExpression> m_overrideExpr;
    Handle<AstVariableDeclaration> m_tempArrayStoreVarDecl;
    const SymbolType* m_exprType;

    Handle<AstArrayAccess> CloneImpl() const
    {
        return MakeHandle<AstArrayAccess>(
            CloneAstNode(m_target),
            CloneAstNode(m_index),
            m_operatorOverloadingEnabled,
            m_location);
    }
};

} // namespace Hyperion
