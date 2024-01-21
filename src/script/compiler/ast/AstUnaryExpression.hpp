#ifndef AST_UNARY_EXPRESSION_HPP
#define AST_UNARY_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Operator.hpp>

namespace hyperion::compiler {

class AstBinaryExpression;

class AstUnaryExpression : public AstExpression
{
public:
    AstUnaryExpression(
        const RC<AstExpression> &target,
        const Operator *op,
        bool is_postfix_version,
        const SourceLocation &location
    );

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstUnaryExpression>());
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());
        hc.Add(m_op ? m_op->GetHashCode() : HashCode());
        hc.Add(m_is_postfix_version);

        return hc;
    }

private:
    RC<AstExpression>   m_target;
    const Operator      *m_op;
    bool                m_is_postfix_version;

    // set while analyzing
    bool                m_folded;

    RC<AstBinaryExpression> m_bin_expr; // internally use a binary expr for somethings (like ++ and -- operators)

    RC<AstUnaryExpression> CloneImpl() const
    {
        return RC<AstUnaryExpression>(new AstUnaryExpression(
            CloneAstNode(m_target),
            m_op,
            m_is_postfix_version,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
