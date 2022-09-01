#ifndef AST_IF_STATEMENT_HPP
#define AST_IF_STATEMENT_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion::compiler {

class AstIfStatement : public AstStatement {
public:
    AstIfStatement(const std::shared_ptr<AstExpression> &conditional,
        const std::shared_ptr<AstBlock> &block,
        const std::shared_ptr<AstBlock> &else_block,
        const SourceLocation &location);
    virtual ~AstIfStatement() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::shared_ptr<AstExpression> m_conditional;
    std::shared_ptr<AstBlock> m_block;
    std::shared_ptr<AstBlock> m_else_block;

    Pointer<AstIfStatement> CloneImpl() const
    {
        return Pointer<AstIfStatement>(new AstIfStatement(
            CloneAstNode(m_conditional),
            CloneAstNode(m_block),
            CloneAstNode(m_else_block),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
