#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Operator.hpp>
#include <Lang/Compiler/Configuration.hpp>

namespace Hyperion {

HYP_CLASS()
class AstBinaryExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstBinaryExpression);

public:
    AstBinaryExpression(
        const Handle<AstExpression>& left,
        const Handle<AstExpression>& right,
        const Operator* op,
        const SourceLocation& location);

    HYP_FORCE_INLINE const Handle<AstExpression>& GetLeft() const
    {
        return m_left;
    }

    HYP_FORCE_INLINE const Handle<AstExpression>& GetRight() const
    {
        return m_right;
    }

    HYP_FORCE_INLINE void SetEnableOverrideExpr(bool enable)
    {
        m_enableOverrideExpr = enable;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual ConstantValue GetConstantValue() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual String ToString() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstBinaryExpression>());
        hc.Add(m_left ? m_left->GetHashCode() : HashCode());
        hc.Add(m_right ? m_right->GetHashCode() : HashCode());
        hc.Add(m_op ? m_op->GetHashCode() : HashCode());

        return hc;
    }

private:
    Handle<AstExpression> m_left;
    Handle<AstExpression> m_right;
    const Operator* m_op;

    Handle<AstExpression> m_overrideExpr;
    bool m_enableOverrideExpr : 1;

#if HYP_SCRIPT_ENABLE_LAZY_DECLARATIONS
    // if the expression is lazy declaration
    Handle<AstVariableDeclaration> m_variableDeclaration;
    Handle<AstVariableDeclaration> CheckLazyDeclaration(AstVisitor* visitor, Module* mod);
#endif

    Handle<AstBinaryExpression> CloneImpl() const
    {
        return MakeHandle<AstBinaryExpression>(
                CloneAstNode(m_left),
                CloneAstNode(m_right),
                m_op,
                m_location);
    }
};

} // namespace Hyperion
