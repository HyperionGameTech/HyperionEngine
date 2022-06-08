#ifndef AST_FOR_RANGE_LOOP_HPP
#define AST_FOR_RANGE_LOOP_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion {
namespace compiler {

class AstForRangeLoop : public AstStatement {
public:
    AstForRangeLoop(
        const std::shared_ptr<AstVariableDeclaration> &decl,
        const std::shared_ptr<AstExpression> &expr,
        const std::shared_ptr<AstBlock> &block,
        const SourceLocation &location
    );
    virtual ~AstForRangeLoop() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::shared_ptr<AstVariableDeclaration> m_decl;
    std::shared_ptr<AstExpression> m_expr;
    std::shared_ptr<AstBlock> m_block;
    int m_num_locals;

    std::shared_ptr<AstVariableDeclaration> m_end_decl;
    std::shared_ptr<AstExpression>          m_conditional,
                                            m_inc_expr;

    inline Pointer<AstForRangeLoop> CloneImpl() const
    {
        return Pointer<AstForRangeLoop>(new AstForRangeLoop(
            CloneAstNode(m_decl),
            CloneAstNode(m_expr),
            CloneAstNode(m_block),
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
