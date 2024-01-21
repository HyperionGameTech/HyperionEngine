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

class AstForLoop : public AstStatement
{
public:
    AstForLoop(
        const RC<AstStatement> &decl_part,
        const RC<AstExpression> &condition_part,
        const RC<AstExpression> &increment_part,
        const RC<AstBlock> &block,
        const SourceLocation &location
    );
    virtual ~AstForLoop() override = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstForLoop>());
        hc.Add(m_decl_part ? m_decl_part->GetHashCode() : HashCode());
        hc.Add(m_condition_part ? m_condition_part->GetHashCode() : HashCode());
        hc.Add(m_increment_part ? m_increment_part->GetHashCode() : HashCode());
        hc.Add(m_block ? m_block->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstStatement>    m_decl_part;
    RC<AstExpression>   m_condition_part;
    RC<AstExpression>   m_increment_part;
    RC<AstBlock>        m_block;

    // set while analyzing
    Int                 m_num_locals;
    Int                 m_num_used_initializers;

    RC<AstExpression>   m_expr;

    RC<AstForLoop> CloneImpl() const
    {
        return RC<AstForLoop>(new AstForLoop(
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
