#ifndef AST_BINARY_EXPRESSION_HPP
#define AST_BINARY_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/Operator.hpp>
#include <script/compiler/Configuration.hpp>

namespace hyperion::compiler {

class AstBinaryExpression : public AstExpression {
public:
    AstBinaryExpression(
        const std::shared_ptr<AstExpression> &left,
        const std::shared_ptr<AstExpression> &right,
        const Operator *op,
        const SourceLocation &location
    );

    const std::shared_ptr<AstExpression> &GetLeft() const { return m_left; }
    const std::shared_ptr<AstExpression> &GetRight() const { return m_right; }

    bool IsOperatorOverloadingEnabled() const
        { return m_operator_overloading_enabled; }
    void SetIsOperatorOverloadingEnabled(bool operator_overloading_enabled)
        { m_operator_overloading_enabled = operator_overloading_enabled; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

private:
    std::shared_ptr<AstExpression> m_left;
    std::shared_ptr<AstExpression> m_right;
    const Operator *m_op;

    std::shared_ptr<AstExpression> m_operator_overload;
    bool m_operator_overloading_enabled;

#if ACE_ENABLE_LAZY_DECLARATIONS
    // if the expression is lazy declaration
    std::shared_ptr<AstVariableDeclaration> m_variable_declaration;
    std::shared_ptr<AstVariableDeclaration> CheckLazyDeclaration(AstVisitor *visitor, Module *mod);
#endif

    std::shared_ptr<AstBinaryExpression> CloneImpl() const
    {
        return std::shared_ptr<AstBinaryExpression>(
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
