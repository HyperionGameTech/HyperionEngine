#ifndef AST_FOR_LOOP_HPP
#define AST_FOR_LOOP_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstForLoop : public AstStatement {
public:
    AstForLoop(
        const std::shared_ptr<AstStatement> &decl_part,
        const std::shared_ptr<AstExpression> &condition_part,
        const std::shared_ptr<AstExpression> &increment_part,
        const std::shared_ptr<AstBlock> &block,
        const SourceLocation &location
    );
    virtual ~AstForLoop() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::shared_ptr<AstStatement> m_decl_part;
    std::shared_ptr<AstExpression> m_condition_part;
    std::shared_ptr<AstExpression> m_increment_part;
    std::shared_ptr<AstBlock> m_block;
    int m_num_locals;
    int m_num_used_initializers;

    std::shared_ptr<AstExpression> m_expr;

    Pointer<AstForLoop> CloneImpl() const
    {
        return Pointer<AstForLoop>(new AstForLoop(
            CloneAstNode(m_decl_part),
            CloneAstNode(m_condition_part),
            CloneAstNode(m_increment_part),
            CloneAstNode(m_block),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
