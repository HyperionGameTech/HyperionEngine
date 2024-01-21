#ifndef AST_WHILE_LOOP_HPP
#define AST_WHILE_LOOP_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion::compiler {

class AstWhileLoop : public AstStatement
{
public:
    AstWhileLoop(
        const RC<AstExpression> &conditional,
        const RC<AstBlock> &block,
        const SourceLocation &location
    );
    virtual ~AstWhileLoop() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstWhileLoop>());
        hc.Add(m_conditional ? m_conditional->GetHashCode() : HashCode());
        hc.Add(m_block ? m_block->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstExpression>   m_conditional;
    RC<AstBlock>        m_block;

    // set while analyzing
    int                 m_num_locals;

    RC<AstWhileLoop> CloneImpl() const
    {
        return RC<AstWhileLoop>(new AstWhileLoop(
            CloneAstNode(m_conditional),
            CloneAstNode(m_block),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
