#ifndef AST_FOR_EACH_LOOP_HPP
#define AST_FOR_EACH_LOOP_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstForEachLoop : public AstStatement
{
public:
    AstForEachLoop(const std::vector<std::shared_ptr<AstParameter>> &params,
        const std::shared_ptr<AstExpression> &iteree,
        const std::shared_ptr<AstBlock> &block,
        const SourceLocation &location);
    virtual ~AstForEachLoop() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::vector<std::shared_ptr<AstParameter>> m_params;
    std::shared_ptr<AstExpression> m_iteree;
    std::shared_ptr<AstBlock> m_block;
    int m_num_locals;

    std::shared_ptr<AstExpression> m_expr;

    Pointer<AstForEachLoop> CloneImpl() const
    {
        return Pointer<AstForEachLoop>(new AstForEachLoop(
            CloneAllAstNodes(m_params),
            CloneAstNode(m_iteree),
            CloneAstNode(m_block),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
