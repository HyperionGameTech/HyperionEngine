#ifndef AST_BINARY_EXPRESSION_HPP
#define AST_BINARY_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/Operator.hpp>
#include <script/compiler/Configuration.hpp>

namespace hyperion::compiler {

class AstBinaryExpression : public AstExpression
{
public:
    AstBinaryExpression(
        const RC<AstExpression> &left,
        const RC<AstExpression> &right,
        const Operator *op,
        const SourceLocation &location
    );

    const RC<AstExpression> &GetLeft() const { return m_left; }
    const RC<AstExpression> &GetRight() const { return m_right; }

    bool IsOperatorOverloadingEnabled() const
        { return m_operator_overloading_enabled; }
    void SetIsOperatorOverloadingEnabled(bool operator_overloading_enabled)
        { m_operator_overloading_enabled = operator_overloading_enabled; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstBinaryExpression>());
        hc.Add(m_left ? m_left->GetHashCode() : HashCode());
        hc.Add(m_right ? m_right->GetHashCode() : HashCode());
        hc.Add(m_op ? m_op->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstExpression>   m_left;
    RC<AstExpression>   m_right;
    const Operator      *m_op;

    RC<AstExpression>   m_operator_overload;
    bool                m_operator_overloading_enabled;

#if HYP_SCRIPT_ENABLE_LAZY_DECLARATIONS
    // if the expression is lazy declaration
    RC<AstVariableDeclaration> m_variable_declaration;
    RC<AstVariableDeclaration> CheckLazyDeclaration(AstVisitor *visitor, Module *mod);
#endif

    RC<AstBinaryExpression> CloneImpl() const
    {
        return RC<AstBinaryExpression>(
            new AstBinaryExpression(
                CloneAstNode(m_left),
                CloneAstNode(m_right),
                m_op,
                m_location
            ));
    }
};

} // namespace hyperion::compiler

#endif
