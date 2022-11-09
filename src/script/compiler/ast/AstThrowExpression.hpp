#ifndef AST_THROW_EXPRESSION_HPP
#define AST_THROW_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion::compiler {

class AstThrowExpression : public AstExpression
{
public:
    AstThrowExpression(
        const std::shared_ptr<AstExpression> &expr,
        const SourceLocation &location
    );
    virtual ~AstThrowExpression() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

protected:
    std::shared_ptr<AstExpression> m_expr;

private:
    Pointer<AstThrowExpression> CloneImpl() const
    {
        return Pointer<AstThrowExpression>(new AstThrowExpression(
            CloneAstNode(m_expr),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
