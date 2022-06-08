#ifndef AST_WHILE_LOOP_HPP
#define AST_WHILE_LOOP_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion {
namespace compiler {

class AstWhileLoop : public AstStatement {
public:
    AstWhileLoop(const std::shared_ptr<AstExpression> &conditional,
        const std::shared_ptr<AstBlock> &block,
        const SourceLocation &location);
    virtual ~AstWhileLoop() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::shared_ptr<AstExpression> m_conditional;
    std::shared_ptr<AstBlock> m_block;
    int m_num_locals;

    inline Pointer<AstWhileLoop> CloneImpl() const
    {
        return Pointer<AstWhileLoop>(new AstWhileLoop(
            CloneAstNode(m_conditional),
            CloneAstNode(m_block),
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
