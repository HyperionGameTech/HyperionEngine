#ifndef AST_UNARY_EXPRESSION_HPP
#define AST_UNARY_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Operator.hpp>

namespace hyperion::compiler {

class AstBinaryExpression;

class AstUnaryExpression : public AstExpression {
public:
    AstUnaryExpression(
        const std::shared_ptr<AstExpression> &target,
        const Operator *op,
        bool is_postfix_version,
        const SourceLocation &location
    );

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

private:
    std::shared_ptr<AstExpression> m_target;
    const Operator *m_op;
    bool m_is_postfix_version;
    bool m_folded;

    std::shared_ptr<AstBinaryExpression> m_bin_expr; // internally use a binary expr for somethings (like ++ and -- operators)

    Pointer<AstUnaryExpression> CloneImpl() const
    {
        return Pointer<AstUnaryExpression>(new AstUnaryExpression(
            CloneAstNode(m_target),
            m_op,
            m_is_postfix_version,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
