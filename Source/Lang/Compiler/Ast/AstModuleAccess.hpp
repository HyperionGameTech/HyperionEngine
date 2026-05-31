#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Core/Containers/String.hpp>

#include <string>
#include <vector>
#include <memory>

namespace Hyperion {

class AstModuleAccess : public AstExpression
{
public:
    AstModuleAccess(
        const String& target,
        const RC<AstExpression>& expr,
        const SourceLocation& location);
    virtual ~AstModuleAccess() override = default;

    HYP_FORCE_INLINE Module* GetModule() const
    {
        return m_modAccess;
    }

    HYP_FORCE_INLINE const String& GetTargetName() const
    {
        return m_target;
    }

    HYP_FORCE_INLINE const RC<AstExpression>& GetExpression() const
    {
        return m_expr;
    }

    HYP_FORCE_INLINE void SetExpression(const RC<AstExpression>& expr)
    {
        m_expr = expr;
    }

    HYP_FORCE_INLINE void SetChained(bool isChained)
    {
        m_isChained = isChained;
    }

    void PerformLookup(AstVisitor* visitor, Module* mod);

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
    virtual bool IsMutable() const override;
    virtual bool IsLiteral() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstModuleAccess>());
        hc.Add(m_target);
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        return hc;
    }

private:
    String m_target;
    RC<AstExpression> m_expr;

    // set while analyzing
    Module* m_modAccess;
    // is this module access chained to another before it?
    bool m_isChained : 1;
    bool m_lookedUp : 1;

    RC<AstModuleAccess> CloneImpl() const
    {
        return RC<AstModuleAccess>(new AstModuleAccess(
            m_target,
            CloneAstNode(m_expr),
            m_location));
    }
};

} // namespace Hyperion
