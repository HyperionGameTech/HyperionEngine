#ifndef AST_TERNARY_EXPRESSION_HPP
#define AST_TERNARY_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion {
namespace compiler {

class AstTernaryExpression : public AstExpression
{
public:
    AstTernaryExpression(
        const RC<AstExpression> &conditional,
        const RC<AstExpression> &left,
        const RC<AstExpression> &right,
        const SourceLocation &location
    );
    virtual ~AstTernaryExpression() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual bool IsLiteral() const override;
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTernaryExpression>());
        hc.Add(m_conditional ? m_conditional->GetHashCode() : HashCode());
        hc.Add(m_left ? m_left->GetHashCode() : HashCode());
        hc.Add(m_right ? m_right->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstExpression>   m_conditional;
    RC<AstExpression>   m_left;
    RC<AstExpression>   m_right;

    RC<AstTernaryExpression> CloneImpl() const
    {
        return RC<AstTernaryExpression>(
            new AstTernaryExpression(
                CloneAstNode(m_conditional),
                CloneAstNode(m_left),
                CloneAstNode(m_right),
                m_location
            ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
